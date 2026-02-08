/*
 * Copyright 2001-2025, Axel Dörfler, axeld@pinc-software.de.
 * This file may be used under the terms of the MIT License.
 */


//! Transaction and logging


#include <StackOrHeapArray.h>

#include "Journal.h"

#include "Debug.h"
#include "Inode.h"


struct run_array {
	int32		count;
	int32		max_runs;
	block_run	runs[0];

	void Init(int32 blockSize);
	void Insert(block_run& run);

	int32 CountRuns() const { return BFS_ENDIAN_TO_HOST_INT32(count); }
	int32 MaxRuns() const { return BFS_ENDIAN_TO_HOST_INT32(max_runs); }
	const block_run& RunAt(int32 i) const { return runs[i]; }

	static int32 MaxRuns(int32 blockSize);

private:
	static int _Compare(block_run& a, block_run& b);
	int32 _FindInsertionIndex(block_run& run);
};

class RunArrays {
public:
							RunArrays(Journal* journal);
							~RunArrays();

			status_t		Insert(off_t blockNumber);

			run_array*		ArrayAt(int32 i) { return fArrays.Array()[i]; }
			int32			CountArrays() const { return fArrays.CountItems(); }

			uint32			CountBlocks() const { return fBlockCount; }
			uint32			LogEntryLength() const
								{ return CountBlocks() + CountArrays(); }

			int32			MaxArrayLength();

private:
			status_t		_AddArray();
			bool			_ContainsRun(block_run& run);
			bool			_AddRun(block_run& run);

			Journal*		fJournal;
			uint32			fBlockCount;
			Stack<run_array*> fArrays;
			run_array*		fLastArray;
};

class LogEntry : public DoublyLinkedListLinkImpl<LogEntry> {
public:
							LogEntry(Journal* journal, uint32 logStart,
								uint32 length, int32 count);
							~LogEntry();

			uint32			Start() const { return fStart; }
			uint32			Length() const { return fLength; }

			int32			DecrementCount() { return atomic_add(&fCount, -1); }

#ifdef BFS_DEBUGGER_COMMANDS
			void			SetTransactionID(int32 id) { fTransactionID = id; }
			int32			TransactionID() const { return fTransactionID; }
#endif

			Journal*		GetJournal() { return fJournal; }

private:
			Journal*		fJournal;
			uint32			fStart;
			uint32			fLength;
			int32			fCount;
#ifdef BFS_DEBUGGER_COMMANDS
			int32			fTransactionID;
#endif
};


#if BFS_TRACING && !defined(FS_SHELL) && !defined(_BOOT_MODE)
namespace BFSJournalTracing {

class LogEntry : public AbstractTraceEntry {
public:
	LogEntry(::LogEntry* entry, off_t logPosition, bool started)
		:
		fEntry(entry),
#ifdef BFS_DEBUGGER_COMMANDS
		fTransactionID(entry->TransactionID()),
#endif
		fStart(entry->Start()),
		fLength(entry->Length()),
		fLogPosition(logPosition),
		fStarted(started)
	{
		Initialized();
	}

	virtual void AddDump(TraceOutput& out)
	{
#ifdef BFS_DEBUGGER_COMMANDS
		out.Print("bfs:j:%s entry %p id %ld, start %lu, length %lu, log %s "
			"%lu\n", fStarted ? "Started" : "Written", fEntry,
			fTransactionID, fStart, fLength,
			fStarted ? "end" : "start", fLogPosition);
#else
		out.Print("bfs:j:%s entry %p start %lu, length %lu, log %s %lu\n",
			fStarted ? "Started" : "Written", fEntry, fStart, fLength,
			fStarted ? "end" : "start", fLogPosition);
#endif
	}

private:
	::LogEntry*	fEntry;
#ifdef BFS_DEBUGGER_COMMANDS
	int32		fTransactionID;
#endif
	uint32		fStart;
	uint32		fLength;
	uint32		fLogPosition;
	bool		fStarted;
};

}	// namespace BFSJournalTracing

#	define T(x) new(std::nothrow) BFSJournalTracing::x;
#else
#	define T(x) ;
#endif


//	#pragma mark -


static void
add_to_iovec(iovec* vecs, int32& index, int32 max, const void* address,
	size_t size)
{
	if (index > 0 && (uint8*)vecs[index - 1].iov_base
			+ vecs[index - 1].iov_len == (uint8*)address) {
		// the iovec can be combined with the previous one
		vecs[index - 1].iov_len += size;
		return;
	}

	if (index == max)
		panic("no more space for iovecs!");

	// we need to start a new iovec
	vecs[index].iov_base = const_cast<void*>(address);
	vecs[index].iov_len = size;
	index++;
}


//	#pragma mark - LogEntry


LogEntry::LogEntry(Journal* journal, uint32 start, uint32 length, int32 count)
	:
	fJournal(journal),
	fStart(start),
	fLength(length),
	fCount(count)
{
}


LogEntry::~LogEntry()
{
}


//	#pragma mark - run_array


/*!	The run_array's size equals the block size of the BFS volume, so we
	cannot use a (non-overridden) new.
	This makes a freshly allocated run_array ready to run.
*/
void
run_array::Init(int32 blockSize)
{
	memset(this, 0, blockSize);
	count = 0;
	max_runs = HOST_ENDIAN_TO_BFS_INT32(MaxRuns(blockSize));
}


/*!	Inserts the block_run into the array. You will have to make sure the
	array is large enough to contain the entry before calling this function.
*/
void
run_array::Insert(block_run& run)
{
	int32 index = _FindInsertionIndex(run);
	if (index == -1) {
		// add to the end
		runs[CountRuns()] = run;
	} else {
		// insert at index
		memmove(&runs[index + 1], &runs[index],
			(CountRuns() - index) * sizeof(off_t));
		runs[index] = run;
	}

	count = HOST_ENDIAN_TO_BFS_INT32(CountRuns() + 1);
}


/*static*/ int32
run_array::MaxRuns(int32 blockSize)
{
	// For whatever reason, BFS restricts the maximum array size
	uint32 maxCount = (blockSize - sizeof(run_array)) / sizeof(block_run);
	if (maxCount < 128)
		return maxCount;

	return 127;
}


/*static*/ int
run_array::_Compare(block_run& a, block_run& b)
{
	int cmp = a.AllocationGroup() - b.AllocationGroup();
	if (cmp == 0)
		return a.Start() - b.Start();

	return cmp;
}


int32
run_array::_FindInsertionIndex(block_run& run)
{
	int32 min = 0, max = CountRuns() - 1;
	int32 i = 0;
	if (max >= 8) {
		while (min <= max) {
			i = (min + max) / 2;

			int cmp = _Compare(runs[i], run);
			if (cmp < 0)
				min = i + 1;
			else if (cmp > 0)
				max = i - 1;
			else
				return -1;
		}

		if (_Compare(runs[i], run) < 0)
			i++;
	} else {
		for (; i <= max; i++) {
			if (_Compare(runs[i], run) > 0)
				break;
		}
		if (i == count)
			return -1;
	}

	return i;
}


//	#pragma mark - RunArrays


RunArrays::RunArrays(Journal* journal)
	:
	fJournal(journal),
	fBlockCount(0),
	fArrays(),
	fLastArray(NULL)
{
}


RunArrays::~RunArrays()
{
	run_array* array;
	while (fArrays.Pop(&array))
		free(array);
}


bool
RunArrays::_ContainsRun(block_run& run)
{
	for (int32 i = 0; i < CountArrays(); i++) {
		run_array* array = ArrayAt(i);

		for (int32 j = 0; j < array->CountRuns(); j++) {
			block_run& arrayRun = array->runs[j];
			if (run.AllocationGroup() != arrayRun.AllocationGroup())
				continue;

			if (run.Start() >= arrayRun.Start()
				&& run.Start() + run.Length()
					<= arrayRun.Start() + arrayRun.Length())
				return true;
		}
	}

	return false;
}


/*!	Adds the specified block_run into the array.
	Note: it doesn't support overlapping - it must only be used
	with block_runs of length 1!
*/
bool
RunArrays::_AddRun(block_run& run)
{
	ASSERT(run.length == 1);

	// Be's BFS log replay routine can only deal with block_runs of size 1
	// A pity, isn't it? Too sad we have to be compatible.

	if (fLastArray == NULL || fLastArray->CountRuns() == fLastArray->MaxRuns())
		return false;

	fLastArray->Insert(run);
	fBlockCount++;
	return true;
}


status_t
RunArrays::_AddArray()
{
	int32 blockSize = fJournal->GetVolume()->BlockSize();

	run_array* array = (run_array*)malloc(blockSize);
	if (array == NULL)
		return B_NO_MEMORY;

	if (fArrays.Push(array) != B_OK) {
		free(array);
		return B_NO_MEMORY;
	}

	array->Init(blockSize);
	fLastArray = array;
	return B_OK;
}


status_t
RunArrays::Insert(off_t blockNumber)
{
	Volume* volume = fJournal->GetVolume();
	block_run run = volume->ToBlockRun(blockNumber);

	if (fLastArray != NULL) {
		// check if the block is already in the array
		if (_ContainsRun(run))
			return B_OK;
	}

	// insert block into array

	if (!_AddRun(run)) {
		// array is full
		if (_AddArray() != B_OK || !_AddRun(run))
			return B_NO_MEMORY;
	}

	return B_OK;
}


int32
RunArrays::MaxArrayLength()
{
	int32 max = 0;
	for (int32 i = 0; i < CountArrays(); i++) {
		if (ArrayAt(i)->CountRuns() > max)
			max = ArrayAt(i)->CountRuns();
	}

	return max;
}


//	#pragma mark - Journal


Journal::Journal(Volume* volume)
	:
	fVolume(volume),
	fOwner(-1),
	fLogSize(volume->Log().Length()),
	fMaxTransactionSize(fLogSize / 2 - 5),
	fUsed(0)
{
	rw_lock_init(&fTransactionLock, "bfs journal transaction");
	recursive_lock_init(&fLogLock, "bfs journal log");
	mutex_init(&fEntriesLock, "bfs journal entries");
}


Journal::~Journal()
{
	FlushLogAndBlocks();

	rw_lock_destroy(&fTransactionLock);
	recursive_lock_destroy(&fLogLock);
	mutex_destroy(&fEntriesLock);
}


status_t
Journal::InitCheck()
{
	if (fLogSize < 12)
		return B_BAD_VALUE;

	return B_OK;
}


/*!	\brief Does a very basic consistency check of the run array.
	It will check the maximum run count as well as if all of the runs fall
	within a the volume.
*/
status_t
Journal::_CheckRunArray(const run_array* array)
{
	int32 maxRuns = run_array::MaxRuns(fVolume->BlockSize());
	if (array->MaxRuns() != maxRuns
		|| array->CountRuns() > maxRuns
		|| array->CountRuns() <= 0) {
		dprintf("run count: %d, array max: %d, max runs: %d\n",
			(int)array->CountRuns(), (int)array->MaxRuns(), (int)maxRuns);
		FATAL(("Log entry has broken header!\n"));
		return B_ERROR;
	}

	for (int32 i = 0; i < array->CountRuns(); i++) {
		if (fVolume->ValidateBlockRun(array->RunAt(i)) != B_OK)
			return B_ERROR;
	}

	PRINT(("Log entry has %" B_PRId32 " entries\n", array->CountRuns()));
	return B_OK;
}


/*!	Replays an entry in the log.
	\a _start points to the entry in the log, and will be bumped to the next
	one if replaying succeeded.
*/
status_t
Journal::_ReplayRunArray(off_t* _start)
{
	PRINT(("ReplayRunArray(start = %" B_PRIdOFF ")\n", *_start));

	off_t logOffset = fVolume->ToBlock(fVolume->Log());
	off_t firstBlockNumber = *_start % fLogSize;

	CachedBlock cachedArray(fVolume);

	status_t status = cachedArray.SetTo(logOffset + firstBlockNumber);
	if (status != B_OK)
		return status;

	const run_array* array = (const run_array*)cachedArray.Block();
	if (_CheckRunArray(array) < B_OK)
		return B_BAD_DATA;

	// First pass: check integrity of the blocks in the run array

	CachedBlock cached(fVolume);

	firstBlockNumber = (firstBlockNumber + 1) % fLogSize;
	off_t blockNumber = firstBlockNumber;
	int32 blockSize = fVolume->BlockSize();

	for (int32 index = 0; index < array->CountRuns(); index++) {
		const block_run& run = array->RunAt(index);

		off_t offset = fVolume->ToOffset(run);
		for (int32 i = 0; i < run.Length(); i++) {
			status = cached.SetTo(logOffset + blockNumber);
			if (status != B_OK)
				RETURN_ERROR(status);

			// TODO: eventually check other well known offsets, like the
			// root and index dirs
			if (offset == 0) {
				// This log entry writes over the superblock - check if
				// it's valid!
				if (Volume::CheckSuperBlock(cached.Block()) != B_OK) {
					FATAL(("Log contains invalid superblock!\n"));
					RETURN_ERROR(B_BAD_DATA);
				}
			}

			blockNumber = (blockNumber + 1) % fLogSize;
			offset += blockSize;
		}
	}

	// Second pass: write back its blocks

	blockNumber = firstBlockNumber;
	int32 count = 1;

	for (int32 index = 0; index < array->CountRuns(); index++) {
		const block_run& run = array->RunAt(index);
		INFORM(("replay block run %u:%u:%u in log at %" B_PRIdOFF "!\n",
			(int)run.AllocationGroup(), run.Start(), run.Length(), blockNumber));

		off_t offset = fVolume->ToOffset(run);
		for (int32 i = 0; i < run.Length(); i++) {
			status = cached.SetTo(logOffset + blockNumber);
			if (status != B_OK)
				RETURN_ERROR(status);

			ssize_t written = write_pos(fVolume->Device(), offset,
				cached.Block(), blockSize);
			if (written != blockSize)
				RETURN_ERROR(B_IO_ERROR);

			blockNumber = (blockNumber + 1) % fLogSize;
			offset += blockSize;
			count++;
		}
	}

	*_start += count;
	return B_OK;
}


/*!	Replays all log entries - this will put the disk into a
	consistent and clean state, if it was not correctly unmounted
	before.
	This method is called by Journal::InitCheck() if the log start
	and end pointer don't match.
*/
status_t
Journal::ReplayLog()
{
	// TODO: this logic won't work whenever the size of the pending transaction
	//	equals the size of the log (happens with the original BFS only)
	if (fVolume->LogStart() == fVolume->LogEnd())
		return B_OK;

	INFORM(("Replay log, disk was not correctly unmounted...\n"));

	if (fVolume->SuperBlock().flags != SUPER_BLOCK_DISK_DIRTY) {
		INFORM(("log_start and log_end differ, but disk is marked clean - "
			"trying to replay log...\n"));
	}

	if (fVolume->IsReadOnly())
		return B_READ_ONLY_DEVICE;

	// Check if the log start and end pointers are valid
	if (fVolume->LogStart() < 0 || (uint32)fVolume->LogStart() > fLogSize
		|| fVolume->LogEnd() < 0 || (uint32)fVolume->LogEnd() > fLogSize) {
		FATAL(("Log pointers are invalid (start = %" B_PRIdOFF
			", end = %" B_PRIdOFF ", size = %" B_PRIu32 ")\n",
			(off_t)fVolume->LogStart(), (off_t)fVolume->LogEnd(), fLogSize));
		return B_BAD_VALUE;
	}

	off_t start = fVolume->LogStart();
	off_t lastStart = -1;
	while (true) {
		// stop if the log is completely flushed
		if (start == fVolume->LogEnd())
			break;

		if (start == lastStart) {
			// strange, flushing the log hasn't changed the log_start pointer
			return B_ERROR;
		}
		lastStart = start;

		status_t status = _ReplayRunArray(&start);
		if (status != B_OK) {
			FATAL(("replaying log entry from %" B_PRIdOFF " failed: %s\n",
				start, strerror(status)));
			return B_ERROR;
		}
		start = start % fLogSize;
	}

	PRINT(("replaying worked fine!\n"));
	fVolume->SuperBlock().log_start = HOST_ENDIAN_TO_BFS_INT64(
		fVolume->LogEnd());
	fVolume->LogStart() = HOST_ENDIAN_TO_BFS_INT64(fVolume->LogEnd());
	fVolume->SuperBlock().flags = HOST_ENDIAN_TO_BFS_INT32(
		SUPER_BLOCK_DISK_CLEAN);

	return fVolume->WriteSuperBlock();
}

size_t
Journal::CurrentTransactionSize(int32 transactionID) const
{
	return cache_blocks_in_main_transaction(fVolume->BlockCache(),
		transactionID);
}


bool
Journal::CurrentTransactionTooLarge(int32 transactionID) const
{
	return CurrentTransactionSize(transactionID) > fLogSize;
}


/*!	This is a callback function that is called by the cache, whenever
	all blocks of a transaction have been flushed to disk.
	This lets us keep track of completed transactions, and update
	the log start pointer as needed. Note, the transactions may not be
	completed in the order they were written.
*/
/*static*/ void
Journal::_TransactionWritten(int32 transactionID, int32 event, void* _logEntry)
{
	LogEntry* logEntry = (LogEntry*)_logEntry;

	if (logEntry->DecrementCount() > 1)
		return;

	PRINT(("Log entry %p has been finished, transaction ID = %" B_PRId32 "\n",
		logEntry, transactionID));

	Journal* journal = logEntry->GetJournal();
	disk_super_block& superBlock = journal->fVolume->SuperBlock();
	bool update = false;

	// Set log_start pointer if possible...

	mutex_lock(&journal->fEntriesLock);

	if (logEntry == journal->fEntries.First()) {
		LogEntry* next = journal->fEntries.GetNext(logEntry);
		if (next != NULL) {
			superBlock.log_start = HOST_ENDIAN_TO_BFS_INT64(next->Start()
				% journal->fLogSize);
		} else {
			superBlock.log_start = HOST_ENDIAN_TO_BFS_INT64(
				journal->fVolume->LogEnd());
		}

		update = true;
	}

	T(LogEntry(logEntry, superBlock.LogStart(), false));

	journal->fUsed -= logEntry->Length();
	journal->fEntries.Remove(logEntry);
	mutex_unlock(&journal->fEntriesLock);

	delete logEntry;

	// update the superblock, and change the disk's state, if necessary

	if (update) {
		if (superBlock.log_start == superBlock.log_end)
			superBlock.flags = HOST_ENDIAN_TO_BFS_INT32(SUPER_BLOCK_DISK_CLEAN);

		status_t status;
		{
			MutexLocker locker(journal->fVolume->Lock());
			status = journal->fVolume->WriteSuperBlock();
		}
		if (status != B_OK) {
			FATAL(("_TransactionWritten: could not write back superblock: %s\n",
				strerror(status)));
		}

		journal->fVolume->LogStart() = superBlock.LogStart();
	}
}


status_t
Journal::_WriteTransactionToLog(int32 transactionID, bool* _transactionEnded)
{
	// Initialize the output flag if provided
	if (_transactionEnded != NULL)
		*_transactionEnded = false;

	// TODO: in case of a failure, we need a backup plan like writing all
	//	 changed blocks back to disk immediately (hello disk corruption!)

	RecursiveLocker locker(fLogLock);

	if (_TransactionSize(transactionID) > fLogSize) {
		// We created a transaction larger than one we can write back to
		// disk - the only option we have (besides risking disk corruption
		// by writing it back anyway), is to let it fail.
		dprintf("transaction too large (%d blocks, log size %d)!\n",
			(int)_TransactionSize(transactionID), (int)fLogSize);
		return B_BUFFER_OVERFLOW;
	}

	int32 blockShift = fVolume->BlockShift();
	off_t logOffset = fVolume->ToBlock(fVolume->Log()) << blockShift;
	off_t logStart = fVolume->LogEnd() % fLogSize;
	off_t logPosition = logStart;

	// Write log entries to disk

	RunArrays runArrays(this);
	status_t status = B_OK;

	off_t blockNumber;
	long cookie = 0;
	while (cache_next_block_in_transaction(fVolume->BlockCache(),
			transactionID, false, &cookie, &blockNumber, NULL,
			NULL) == B_OK) {
		status = runArrays.Insert(blockNumber);
		if (status < B_OK) {
			FATAL(("filling log entry failed!"));
			return status;
		}
	}

	if (runArrays.CountBlocks() == 0) {
		// nothing has changed during this transaction
		// We mark it as ended because cache_end_transaction consumes the ID
		if (_transactionEnded != NULL)
			*_transactionEnded = true;

		return cache_end_transaction(fVolume->BlockCache(), transactionID, NULL,
			NULL);
	}

	// If necessary, flush the log, so that we have enough space for this
	// transaction
	if (runArrays.LogEntryLength() > FreeLogBlocks()) {
		status_t syncStatus = cache_sync_transaction(fVolume->BlockCache(),
			transactionID - 1);
		if (syncStatus != B_OK) {
			dprintf("cache_sync_transaction failed: %s\n",
				strerror(syncStatus));
		}
		if (runArrays.LogEntryLength() > FreeLogBlocks()) {
			// Even after syncing the previous transaction, there is still
			// not enough space in the log for this transaction. We cannot
			// complete it, so we have to fail.
			dprintf("bfs: no space in log after sync (%ld for %ld blocks)!",
				(long)FreeLogBlocks(), (long)runArrays.LogEntryLength());
			return B_DEVICE_FULL;
		}
	}

	int32 maxVecs = runArrays.MaxArrayLength() + 1;
		// one extra for the index block

	BStackOrHeapArray<iovec, 8> vecs(maxVecs);
	if (!vecs.IsValid()) {
		// TODO: write back log entries directly?
		return B_NO_MEMORY;
	}

	for (int32 k = 0; k < runArrays.CountArrays(); k++) {
		run_array* array = runArrays.ArrayAt(k);
		int32 index = 0, count = 1;
		int32 wrap = fLogSize - logStart;

		add_to_iovec(vecs, index, maxVecs, (void*)array, fVolume->BlockSize());

		// add block runs

		for (int32 i = 0; i < array->CountRuns(); i++) {
			const block_run& run = array->RunAt(i);
			off_t blockNumber = fVolume->ToBlock(run);

			for (int32 j = 0; j < run.Length(); j++) {
				if (count >= wrap) {
					// We need to write back the first half of the entry
					// directly as the log wraps around
					if (writev_pos(fVolume->Device(), logOffset
							+ (logStart << blockShift), vecs, index) < 0)
						FATAL(("could not write log area!\n"));

					logPosition = logStart + count;
					logStart = 0;
					wrap = fLogSize;
					count = 0;
					index = 0;
				}

				// make blocks available in the cache
				const void* data = block_cache_get(fVolume->BlockCache(),
					blockNumber + j);
				if (data == NULL)
					return B_IO_ERROR;

				add_to_iovec(vecs, index, maxVecs, data, fVolume->BlockSize());
				count++;
			}
		}

		// write back the rest of the log entry
		if (count > 0) {
			logPosition = logStart + count;
			if (writev_pos(fVolume->Device(), logOffset
					+ (logStart << blockShift), vecs, index) < 0)
				FATAL(("could not write log area: %s!\n", strerror(errno)));
		}

		// release blocks again
		for (int32 i = 0; i < array->CountRuns(); i++) {
			const block_run& run = array->RunAt(i);
			off_t blockNumber = fVolume->ToBlock(run);

			for (int32 j = 0; j < run.Length(); j++) {
				block_cache_put(fVolume->BlockCache(), blockNumber + j);
			}
		}

		logStart = logPosition % fLogSize;
	}

	LogEntry* logEntry = new(std::nothrow) LogEntry(this, fVolume->LogEnd(),
		runArrays.LogEntryLength(), 1);
	if (logEntry == NULL) {
		FATAL(("no memory to allocate log entries!"));
		return B_NO_MEMORY;
	}

#ifdef BFS_DEBUGGER_COMMANDS
	logEntry->SetTransactionID(transactionID);
#endif

	// Update the log end pointer in the superblock

	fVolume->SuperBlock().flags = SUPER_BLOCK_DISK_DIRTY;
	fVolume->SuperBlock().log_end = HOST_ENDIAN_TO_BFS_INT64(logPosition);

	status_t writeStatus;
	{
		MutexLocker locker(fVolume->Lock());
		writeStatus = fVolume->WriteSuperBlock();
	}
	if (writeStatus != B_OK) {
		FATAL(("_WriteTransactionToLog: could not write back superblock: %s\n",
			strerror(writeStatus)));
		// We must not return the error here if cache_end_transaction() succeeds
		// later, because the caller would try to abort the transaction even
		// though it has already been ended.
	}

	fVolume->LogEnd() = logPosition;
	T(LogEntry(logEntry, fVolume->LogEnd(), true));

	// We need to flush the drives own cache here to ensure
	// disk consistency.
	// If that call fails, we can't do anything about it anyway
	ioctl(fVolume->Device(), B_FLUSH_DRIVE_CACHE);

	// at this point, we can finally end the transaction - we're in
	// a guaranteed valid state

	mutex_lock(&fEntriesLock);
	fEntries.Add(logEntry);
	fUsed += logEntry->Length();
	mutex_unlock(&fEntriesLock);

	// FLAG TRANSACTION AS ENDED
	// Even if cache_end_transaction returns an error, it consumes the ID.
	if (_transactionEnded != NULL)
		*_transactionEnded = true;

	status_t endStatus = cache_end_transaction(fVolume->BlockCache(),
		transactionID, _TransactionWritten, logEntry);
	if (endStatus != B_OK) {
		// If the transaction cannot be ended, we need to try to sync the
		// previous transactions, so that we can free up some memory.
		cache_sync_transaction(fVolume->BlockCache(), transactionID - 1);
		endStatus = cache_end_transaction(fVolume->BlockCache(), transactionID,
			_TransactionWritten, logEntry);
		if (endStatus != B_OK)
			panic("cache_end_transaction failed: %s", strerror(endStatus));
	}

	return endStatus;
}


/*!	Flushes the current log entry to disk. If \a flushBlocks is \c true it will
	also write back all dirty blocks for this volume. If \a alreadyLocked is \c
	true, we allow the lock to be held when the function is called.
*/
status_t
Journal::_FlushLog(bool flushBlocks)
{
	RecursiveLocker locker(fLogLock);

	if (flushBlocks)
		return fVolume->FlushDevice();

	return B_OK;
}


/*!	Flushes the current log entry to disk, and also writes back all dirty
	blocks for this volume (completing all open transactions).
*/
status_t
Journal::FlushLogAndBlocks()
{
	return _FlushLog(true);
}


/*!	Locks the journal, in addition to flushing the log and blocks. A return
	value of \c B_OK indicates that the operation was successful, and that
	the journal is locked.
*/
status_t
Journal::FlushLogAndLockJournal()
{
	status_t status = rw_lock_write_lock(&fTransactionLock);
	if (status != B_OK)
		return status;

	fOwner = find_thread(NULL);

	status = _FlushLog(true);

	if (status != B_OK) {
		fOwner = -1;
		rw_lock_write_unlock(&fTransactionLock);
	}

	return status;
}


status_t
Journal::Lock(Transaction* owner)
{
	if (owner == NULL)
		return B_BAD_VALUE;

	mutex_lock(&fEntriesLock);

	// check if this thread already has an active transaction
	thread_id thread = find_thread(NULL);
	DoublyLinkedList<Transaction, DoublyLinkedListMemberGetLink<Transaction,
		&Transaction::fActiveLink> >::Iterator iterator
		= fActiveTransactions.GetIterator();
	while (Transaction* transaction = iterator.Next()) {
		if (transaction->Thread() == thread) {
			// there is already a transaction for this thread
			owner->fParent = transaction;
			owner->fTransactionID = transaction->ID();
			mutex_unlock(&fEntriesLock);
			return B_OK;
		}
	}

	if (fOwner == thread) {
		// We already own the journal lock (via FlushLogAndLockJournal)
		// we can just proceed and pretend we have a transaction, but we
		// shouldn't try to acquire the rw_lock again.
		owner->fTransactionID = cache_start_transaction(fVolume->BlockCache());
		if (owner->fTransactionID < B_OK) {
			mutex_unlock(&fEntriesLock);
			return owner->fTransactionID;
		}

		owner->fThread = thread;
		fActiveTransactions.Add(owner);
		mutex_unlock(&fEntriesLock);
		return B_OK;
	}

	mutex_unlock(&fEntriesLock);

	// No active transaction for this thread, start a new one.
	// We acquire the transaction lock for reading, so that
	// FlushLogAndLockJournal() can block us if needed.
	status_t status = rw_lock_read_lock(&fTransactionLock);
	if (status != B_OK)
		return status;

	// Check if someone else started a transaction for us while we were
	// waiting for the lock (unlikely but possible if we release fEntriesLock)
	// Actually, we don't need to check again because transactions are
	// thread-local and we are the thread.

	owner->fTransactionID = cache_start_transaction(fVolume->BlockCache());
	if (owner->fTransactionID < B_OK) {
		rw_lock_read_unlock(&fTransactionLock);
		return owner->fTransactionID;
	}

	owner->fThread = thread;

	mutex_lock(&fEntriesLock);
	fActiveTransactions.Add(owner);
	mutex_unlock(&fEntriesLock);

	return B_OK;
}

status_t
Journal::Unlock(Transaction* owner, bool success)
{
	// Unlock(NULL, ...) implies unlocking the global journal lock
	// acquired by FlushLogAndLockJournal()
	if (owner == NULL) {
		fOwner = -1;
		rw_lock_write_unlock(&fTransactionLock);
		return B_OK;
	}

	if (owner->Parent() != NULL) {
		// we are a nested transaction
		owner->MoveListenersTo(owner->Parent());
		return B_OK;
	}

	mutex_lock(&fEntriesLock);
	fActiveTransactions.Remove(owner);
	mutex_unlock(&fEntriesLock);

	status_t status = B_OK;
	bool transactionEnded = false;

	if (success) {
		// Pass the tracker to _WriteTransactionToLog so we know if it
		// closed the transaction ID internally.
		status = _WriteTransactionToLog(owner->ID(), &transactionEnded);
	}

	// CRITICAL FIX:
	// Only abort if the transaction was NOT ended by the write attempt.
	// If _WriteTransactionToLog failed but set transactionEnded to true,
	// the ID is already dead and calling abort would crash the system.
	if (status != B_OK && !transactionEnded) {
		cache_abort_transaction(fVolume->BlockCache(), owner->ID());
	} else if (!success && !transactionEnded) {
		// Standard abort case: user requested failure
		cache_abort_transaction(fVolume->BlockCache(), owner->ID());
	}

	if (fOwner != find_thread(NULL))
		rw_lock_read_unlock(&fTransactionLock);

	owner->NotifyListeners(success && status == B_OK);
	fTimestamp = system_time();

	return status;
}

uint32
Journal::_TransactionSize(int32 transactionID) const
{
	int32 count = cache_blocks_in_transaction(fVolume->BlockCache(),
		transactionID);
	if (count <= 0)
		return 0;

	// take the number of array blocks in this transaction into account
	uint32 maxRuns = run_array::MaxRuns(fVolume->BlockSize());
	uint32 arrayBlocks = (count + maxRuns - 1) / maxRuns;
	return count + arrayBlocks;
}


status_t
Journal::MoveLog(block_run newLog)
{
	block_run oldLog = fVolume->Log();
	if (newLog == oldLog)
		return B_OK;

	off_t newEnd = newLog.Start() + newLog.Length();
	off_t oldEnd = oldLog.Start() + oldLog.Length();

	// make sure the new log position is ok
	if (newLog.AllocationGroup() != 0)
		return B_BAD_VALUE;

	if (fVolume->ValidateBlockRun(newLog) != B_OK)
		return B_BAD_VALUE;

	if (newLog.Start() < 1 + fVolume->NumBitmapBlocks())
		return B_BAD_VALUE;

	if (newEnd > fVolume->NumBlocks())
		return B_BAD_VALUE;

	status_t status;
	block_run allocatedRun = {};

	BlockAllocator& allocator = fVolume->Allocator();

	// allocate blocks if necessary
	if (newEnd > oldEnd) {
		if (oldEnd > newLog.Start())
			allocatedRun.SetTo(newLog.AllocationGroup(), oldEnd, newEnd - oldEnd);
		else
			allocatedRun = newLog;

		Transaction transaction(fVolume, 0);
		if (!transaction.IsStarted())
			return B_ERROR;

		status = allocator.AllocateBlockRun(transaction, allocatedRun);
		if (status != B_OK) {
			FATAL(("MoveLog: Could not allocate space to move log area!\n"));
			return status;
		}

		status = transaction.Done();
		if (status != B_OK)
			return status;
	}

	MutexLocker volumeLock(fVolume->Lock());

	status = FlushLogAndLockJournal();
	if (status != B_OK)
		return status;

	// update references to the log location and size
	fVolume->SuperBlock().log_blocks = newLog;
	status = fVolume->WriteSuperBlock();
	if (status != B_OK) {
		fVolume->SuperBlock().log_blocks = oldLog;

		rw_lock_write_unlock(&fTransactionLock);

		// if we had to allocate some blocks, try to free them
		if (!allocatedRun.IsZero()) {
			Transaction transaction(fVolume, 0);
			status_t freeStatus = B_ERROR;
			if (transaction.IsStarted())
				freeStatus = allocator.Free(transaction, allocatedRun);

			if (freeStatus == B_OK)
				freeStatus = transaction.Done();

			// don't really care if we fail
			if (freeStatus != B_OK)
				REPORT_ERROR(freeStatus);
		}

		return status;
	}

	fLogSize = newLog.Length();
	fMaxTransactionSize = fLogSize / 2 - 5;

	rw_lock_write_unlock(&fTransactionLock);
	volumeLock.Unlock();

	// at this point, the log is moved and functional in its new location

	// free blocks if necessary
	if (newEnd < oldEnd) {
		block_run runToFree = block_run::Run(0, newEnd, oldEnd - newEnd);

		Transaction transaction(fVolume, 0);
		if (!transaction.IsStarted())
			return B_ERROR;

		status = allocator.Free(transaction, runToFree);
		if (status == B_OK)
			status = transaction.Done();

		// we've already moved the log, no sense in failing just because we
		// couldn't free a couple of blocks
		if (status != B_OK)
			REPORT_ERROR(status);
	}

	return B_OK;
}


//	#pragma mark - debugger commands


#ifdef BFS_DEBUGGER_COMMANDS


void
Journal::Dump()
{
	kprintf("Journal %p\n", this);
	kprintf("  log start:            %" B_PRId32 "\n", fVolume->LogStart());
	kprintf("  log end:              %" B_PRId32 "\n", fVolume->LogEnd());
	kprintf("  log size:             %" B_PRIu32 "\n", fLogSize);
	kprintf("  max transaction size: %" B_PRIu32 "\n", fMaxTransactionSize);
	kprintf("  used:                 %" B_PRIu32 "\n", fUsed);
	kprintf("  timestamp:            %" B_PRId64 "\n", fTimestamp);
	kprintf("entries:\n");
	kprintf("  address        id  start length\n");

	LogEntryList::Iterator iterator = fEntries.GetIterator();

	while (iterator.HasNext()) {
		LogEntry* entry = iterator.Next();

		kprintf("  %p %6" B_PRId32 " %6" B_PRIu32 " %6" B_PRIu32 "\n", entry,
			entry->TransactionID(), entry->Start(), entry->Length());
	}
}


int
dump_journal(int argc, char** argv)
{
	if (argc != 2 || !strcmp(argv[1], "--help")) {
		kprintf("usage: %s <ptr-to-volume>\n", argv[0]);
		return 0;
	}

	Volume* volume = (Volume*)parse_expression(argv[1]);
	Journal* journal = volume->GetJournal(0);

	journal->Dump();
	return 0;
}


#endif	// BFS_DEBUGGER_COMMANDS


//	#pragma mark - TransactionListener


TransactionListener::TransactionListener()
{
}


TransactionListener::~TransactionListener()
{
}


//	#pragma mark - Transaction


status_t
Transaction::Start(Volume* volume, off_t refBlock)
{
	// has it already been started?
	if (fJournal != NULL)
		return B_OK;

	fJournal = volume->GetJournal(refBlock);
	if (fJournal == NULL)
		return B_ERROR;

	status_t status = fJournal->Lock(this);
	if (status != B_OK)
		fJournal = NULL;

	return status;
}


void
Transaction::AddListener(TransactionListener* listener)
{
	if (fJournal == NULL)
		panic("Transaction is not running!");

	fListeners.Add(listener);
}


void
Transaction::RemoveListener(TransactionListener* listener)
{
	if (fJournal == NULL)
		panic("Transaction is not running!");

	fListeners.Remove(listener);
	listener->RemovedFromTransaction();
}


void
Transaction::MoveListenersTo(Transaction* transaction)
{
	while (TransactionListener* listener = fListeners.RemoveHead()) {
		transaction->fListeners.Add(listener);
	}
}


void
Transaction::NotifyListeners(bool success)
{
	while (TransactionListener* listener = fListeners.Last()) {
		fListeners.Remove(listener);
		listener->TransactionDone(success);
		listener->RemovedFromTransaction();
	}
}


status_t
Transaction::WriteBlocks(off_t blockNumber, const uint8* buffer,
	size_t numBlocks)
{
	if (fJournal == NULL)
		return B_NO_INIT;

	void* cache = GetVolume()->BlockCache();
	size_t blockSize = GetVolume()->BlockSize();

	for (size_t i = 0; i < numBlocks; i++) {
		void* block = block_cache_get_empty(cache, blockNumber + i,
			ID());
		if (block == NULL)
			return B_ERROR;

		memcpy(block, buffer, blockSize);
		buffer += blockSize;

		block_cache_put(cache, blockNumber + i);
	}

	return B_OK;
}
