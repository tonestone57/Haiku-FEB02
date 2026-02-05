/*
 * Copyright 2001-2025, Axel Dörfler, axeld@pinc-software.de.
 * This file may be used under the terms of the MIT License.
 */


//! Transaction and logging


#include <StackOrHeapArray.h>
#include <util/AutoLock.h>

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
	int32 MaxRuns() const { return BFS_ENDIAN_TO_HOST_INT32(max_runs) - 1; }
		// that -1 accounts for an off-by-one error in Be's BFS implementation
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
	if (index > 0 && (addr_t)vecs[index - 1].iov_base
			+ vecs[index - 1].iov_len == (addr_t)address) {
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
	fLogSize(volume->Log().Length()),
	fMaxTransactionSize(fLogSize / 2 - 5),
	fUsed(0),
	fPendingTransactionCount(0)
{
	recursive_lock_init(&fLock, "bfs journal");
	mutex_init(&fEntriesLock, "bfs journal entries");
	mutex_init(&fTransactionMapLock, "bfs journal transactions");

	fInitStatus = fTransactionMap.Init();

	fLogFlusherSem = create_sem(0, "bfs log flusher");
	fLogFlusher = spawn_kernel_thread(&Journal::_LogFlusher, "bfs log flusher",
		B_NORMAL_PRIORITY, this);
	if (fLogFlusher > 0)
		resume_thread(fLogFlusher);
}


Journal::~Journal()
{
	FlushLogAndBlocks();

	recursive_lock_destroy(&fLock);
	mutex_destroy(&fEntriesLock);
	mutex_destroy(&fTransactionMapLock);

	sem_id logFlusher = fLogFlusherSem;
	fLogFlusherSem = -1;
	delete_sem(logFlusher);
	wait_for_thread(fLogFlusher, NULL);
}


status_t
Journal::InitCheck()
{
	return fInitStatus;
}


/*!	\brief Does a very basic consistency check of the run array.
	It will check the maximum run count as well as if all of the runs fall
	within a the volume.
*/
status_t
Journal::_CheckRunArray(const run_array* array)
{
	int32 maxRuns = run_array::MaxRuns(fVolume->BlockSize()) - 1;
		// the -1 works around an off-by-one bug in Be's BFS implementation,
		// same as in run_array::MaxRuns()
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
Journal::_ReplayRunArray(int32* _start)
{
	PRINT(("ReplayRunArray(start = %" B_PRId32 ")\n", *_start));

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

	int32 start = fVolume->LogStart();
	int32 lastStart = -1;
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
			FATAL(("replaying log entry from %d failed: %s\n", (int)start,
				strerror(status)));
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
Journal::TransactionSize(int32 transactionID) const
{
	return cache_blocks_in_main_transaction(fVolume->BlockCache(),
		transactionID);
}


bool
Journal::TransactionTooLarge(int32 transactionID) const
{
	return TransactionSize(transactionID) > fLogSize;
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

		status_t status = journal->fVolume->WriteSuperBlock();
		if (status != B_OK) {
			FATAL(("_TransactionWritten: could not write back superblock: %s\n",
				strerror(status)));
		}

		journal->fVolume->LogStart() = superBlock.LogStart();
	}
}


/*!	Listens to TRANSACTION_IDLE events, and flushes the log when that happens */
/*static*/ void
Journal::_TransactionIdle(int32 transactionID, int32 event, void* _journal)
{
	// The current transaction seems to be idle - flush it. (We can't do this
	// in this thread, as flushing the log can produce new transaction events.)
	Journal* journal = (Journal*)_journal;
	release_sem(journal->fLogFlusherSem);
}


/*static*/ status_t
Journal::_LogFlusher(void* _journal)
{
	Journal* journal = (Journal*)_journal;
	while (journal->fLogFlusherSem >= 0) {
		if (acquire_sem(journal->fLogFlusherSem) != B_OK)
			continue;

		journal->_FlushLog(false, false);
	}
	return B_OK;
}


/*!	Writes the blocks that are part of current transaction into the log,
	and ends the current transaction.
	If the current transaction is too large to fit into the log, it will
	try to detach an existing sub-transaction.
*/
status_t
Journal::_FlushPendingTransactions()
{
	if (fPendingTransactionCount == 0)
		return B_OK;

	// TODO: in case of a failure, we need a backup plan like writing all
	//	changed blocks back to disk immediately (hello disk corruption!)

	RunArrays runArrays(this);
	status_t status = B_OK;

	for (int32 i = 0; i < fPendingTransactionCount; i++) {
		int32 transactionID = fPendingTransactions[i];
		off_t blockNumber;
		long cookie = 0;
		while (cache_next_block_in_transaction(fVolume->BlockCache(),
				transactionID, false, &cookie, &blockNumber, NULL,
				NULL) == B_OK) {
			status = runArrays.Insert(blockNumber);
			if (status < B_OK) {
				FATAL(("filling log entry failed!"));
				// We can't recover easily from this, but we must clear pending count
				// to avoid infinite loops or memory corruption.
				for (int32 j = 0; j < fPendingTransactionCount; j++) {
					cache_abort_transaction(fVolume->BlockCache(),
						fPendingTransactions[j]);
				}
				fPendingTransactionCount = 0;
				return status;
			}
		}
	}

	if (runArrays.CountBlocks() == 0) {
		// nothing has changed during these transactions
		for (int32 i = 0; i < fPendingTransactionCount; i++) {
			cache_end_transaction(fVolume->BlockCache(), fPendingTransactions[i],
				NULL, NULL);
		}
		fPendingTransactionCount = 0;
		return B_OK;
	}

	// If necessary, flush the log, so that we have enough space for this
	// transaction
	if (runArrays.LogEntryLength() > FreeLogBlocks()) {
		// we just flush the first pending transaction to free some space
		cache_sync_transaction(fVolume->BlockCache(), fPendingTransactions[0]);
		if (runArrays.LogEntryLength() > FreeLogBlocks()) {
			panic("no space in log after sync (%ld for %ld blocks)!",
				(long)FreeLogBlocks(), (long)runArrays.LogEntryLength());
		}
	}

	int32 blockShift = fVolume->BlockShift();
	off_t logOffset = fVolume->ToBlock(fVolume->Log()) << blockShift;
	off_t logStart = fVolume->LogEnd() % fLogSize;
	off_t logPosition = logStart;

	// Write log entries to disk

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
		runArrays.LogEntryLength(), fPendingTransactionCount);
	if (logEntry == NULL) {
		FATAL(("no memory to allocate log entries!"));
		return B_NO_MEMORY;
	}

#ifdef BFS_DEBUGGER_COMMANDS
	logEntry->SetTransactionID(fPendingTransactions[fPendingTransactionCount - 1]);
#endif

	// Update the log end pointer in the superblock

	fVolume->SuperBlock().flags = SUPER_BLOCK_DISK_DIRTY;
	fVolume->SuperBlock().log_end = HOST_ENDIAN_TO_BFS_INT64(logPosition);

	status = fVolume->WriteSuperBlock();

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

	for (int32 i = 0; i < fPendingTransactionCount; i++) {
		cache_end_transaction(fVolume->BlockCache(), fPendingTransactions[i],
			_TransactionWritten, logEntry);
	}

	fPendingTransactionCount = 0;
	return status;
}


/*!	Flushes the current log entry to disk. If \a flushBlocks is \c true it will
	also write back all dirty blocks for this volume. If \a alreadyLocked is \c
	true, we allow the lock to be held when the function is called.
*/
status_t
Journal::_FlushLog(bool canWait, bool flushBlocks, bool alreadyLocked)
{
	status_t status = canWait ? recursive_lock_lock(&fLock)
		: recursive_lock_trylock(&fLock);
	if (status != B_OK)
		return status;

	// write the current log entry to disk

	if (fPendingTransactionCount > 0) {
		status = _FlushPendingTransactions();
		if (status < B_OK)
			FATAL(("writing current log entry failed: %s\n", strerror(status)));
	}

	if (flushBlocks)
		status = fVolume->FlushDevice();

	recursive_lock_unlock(&fLock);
	return status;
}


/*!	Flushes the current log entry to disk, and also writes back all dirty
	blocks for this volume (completing all open transactions).
*/
status_t
Journal::FlushLogAndBlocks()
{
	return _FlushLog(true, true);
}


/*!	Locks the journal, in addition to flushing the log and blocks. A return
	value of \c B_OK indicates that the operation was successful, and that
	the journal is locked.
*/
status_t
Journal::FlushLogAndLockJournal()
{
	status_t status = recursive_lock_lock(&fLock);
	if (status != B_OK)
		return status;

	status = _FlushLog(true, true, true);

	if (status != B_OK)
		recursive_lock_unlock(&fLock);

	return status;
}


int32
Journal::StartTransaction(Transaction* owner)
{
	MutexLocker locker(fTransactionMapLock);

	TransactionInfo* info = fTransactionMap.Lookup(find_thread(NULL));
	if (info != NULL) {
		// Nested transaction
		info->nesting++;
		return info->transactionID;
	}

	int32 transactionID = cache_start_transaction(fVolume->BlockCache());
	if (transactionID < B_OK)
		return transactionID;

	info = new(std::nothrow) TransactionInfo;
	if (info == NULL) {
		cache_abort_transaction(fVolume->BlockCache(), transactionID);
		return B_NO_MEMORY;
	}

	info->thread = find_thread(NULL);
	info->transactionID = transactionID;
	info->nesting = 1;
	info->failed = false;
	fTransactionMap.Insert(info);

	cache_add_transaction_listener(fVolume->BlockCache(), transactionID,
		TRANSACTION_IDLE, _TransactionIdle, this);

	return transactionID;
}


status_t
Journal::CommitTransaction(Transaction* owner, bool success)
{
	MutexLocker locker(fTransactionMapLock);

	TransactionInfo* info = fTransactionMap.Lookup(find_thread(NULL));
	if (info == NULL) {
		panic("CommitTransaction: no transaction for thread %" B_PRId32,
			find_thread(NULL));
		return B_BAD_VALUE;
	}

	if (!success)
		info->failed = true;

	if (--info->nesting > 0)
		return B_OK;

	fTransactionMap.Remove(info);
	int32 transactionID = info->transactionID;
	bool failed = info->failed;

	// Move listeners from info to the owner transaction so they can be notified
	while (TransactionListener* listener = info->listeners.RemoveHead()) {
		owner->fListeners.Add(listener);
	}

	delete info;

	locker.Unlock();

	status_t status = _TransactionDone(transactionID, !failed);
	if (status != B_OK)
		return status;

	owner->NotifyListeners(!failed);

	return B_OK;
}


void
Journal::AddTransactionListener(Transaction* owner, TransactionListener* listener)
{
	MutexLocker locker(fTransactionMapLock);

	TransactionInfo* info = fTransactionMap.Lookup(find_thread(NULL));
	if (info != NULL) {
		info->listeners.Add(listener);
	} else {
		// Should not happen if StartTransaction was called
		panic("AddTransactionListener: no transaction for thread");
	}
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
Journal::_TransactionDone(int32 transactionID, bool success)
{
	if (!success) {
		cache_abort_transaction(fVolume->BlockCache(), transactionID);
		return B_OK;
	}

	RecursiveLocker _(fLock);

	fTimestamp = system_time();

	if (_TransactionSize(transactionID) > fLogSize) {
		// This transaction is too large to fit into the log
		dprintf("transaction too large (%d blocks, log size %d)!\n",
			(int)_TransactionSize(transactionID), (int)fLogSize);
		return B_BUFFER_OVERFLOW;
	}

	if (fPendingTransactionCount >= 64) {
		status_t status = _FlushPendingTransactions();
		if (status != B_OK) {
			cache_abort_transaction(fVolume->BlockCache(), transactionID);
			return status;
		}
	}

	fPendingTransactions[fPendingTransactionCount++] = transactionID;

	if (_TransactionSize(transactionID) >= fMaxTransactionSize) {
		status_t status = _FlushPendingTransactions();
		if (status != B_OK)
			return status;
	}

	return B_OK;
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

		recursive_lock_unlock(&fLock);

		// if we had to allocate some blocks, try to free them
		if (!allocatedRun.IsZero()) {
			Transaction transaction(fVolume, 0);
			status_t freeStatus = allocator.Free(transaction, allocatedRun);
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

	recursive_lock_unlock(&fLock);
	volumeLock.Unlock();

	// at this point, the log is moved and functional in its new location

	// free blocks if necessary
	if (newEnd < oldEnd) {
		block_run runToFree = block_run::Run(0, newEnd, oldEnd - newEnd);

		Transaction transaction(fVolume, 0);

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
	if (fJournal != NULL) {
		fTransactionID = fJournal->StartTransaction(this);
		if (fTransactionID >= B_OK)
			return B_OK;
	}

	fJournal = NULL;
	return B_ERROR;
}


void
Transaction::AddListener(TransactionListener* listener)
{
	if (fJournal == NULL)
		panic("Transaction is not running!");

	fJournal->AddTransactionListener(this, listener);
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
Transaction::NotifyListeners(bool success)
{
	while (TransactionListener* listener = fListeners.RemoveHead()) {
		listener->TransactionDone(success);
		listener->RemovedFromTransaction();
	}
}
