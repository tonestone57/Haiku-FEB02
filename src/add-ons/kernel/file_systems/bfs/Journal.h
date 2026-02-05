/*
 * Copyright 2001-2012, Axel Dörfler, axeld@pinc-software.de.
 * This file may be used under the terms of the MIT License.
 */
#ifndef JOURNAL_H
#define JOURNAL_H


#include "system_dependencies.h"

#include <util/OpenHashTable.h>

#include "Volume.h"
#include "Utility.h"


struct run_array;
class Inode;
class LogEntry;
typedef DoublyLinkedList<LogEntry> LogEntryList;


class TransactionListener
	: public DoublyLinkedListLinkImpl<TransactionListener> {
public:
								TransactionListener();
	virtual						~TransactionListener();

	virtual void				TransactionDone(bool success) = 0;
	virtual void				RemovedFromTransaction() = 0;
};

typedef DoublyLinkedList<TransactionListener> TransactionListeners;


struct TransactionInfo {
	thread_id		thread;
	int32			transactionID;
	int32			nesting;
	bool			failed;
	TransactionListeners listeners;
	TransactionInfo* next;
};


struct TransactionMapHash {
	typedef thread_id		KeyType;
	typedef	TransactionInfo	ValueType;

	size_t HashKey(KeyType key) const
	{
		return key;
	}

	size_t Hash(ValueType* value) const
	{
		return HashKey(value->thread);
	}

	bool Compare(KeyType key, ValueType* value) const
	{
		return value->thread == key;
	}

	ValueType*& GetLink(ValueType* value) const
	{
		return value->next;
	}
};


class Journal {
public:
							Journal(Volume* volume);
							~Journal();

			status_t		InitCheck();

			int32			StartTransaction(Transaction* owner);
			status_t		CommitTransaction(Transaction* owner, bool success);
			void			AddTransactionListener(Transaction* owner,
								TransactionListener* listener);

			status_t		ReplayLog();

			size_t			TransactionSize(int32 transactionID) const;
			bool			TransactionTooLarge(int32 transactionID) const;

			status_t		FlushLogAndBlocks();
			status_t		FlushLogAndLockJournal();

			Volume*			GetVolume() const { return fVolume; }

	inline	uint32			FreeLogBlocks() const;

			status_t		MoveLog(block_run newLog);

#ifdef BFS_DEBUGGER_COMMANDS
			void			Dump();
#endif

private:
			status_t		_FlushLog(bool canWait, bool flushBlocks,
								bool alreadyLocked = false);
			status_t		_FlushPendingTransactions();
			uint32			_TransactionSize(int32 transactionID) const;
			status_t		_CheckRunArray(const run_array* array);
			status_t		_ReplayRunArray(int32* start);
			status_t		_TransactionDone(int32 transactionID, bool success);

	static	void			_TransactionWritten(int32 transactionID,
								int32 event, void* _logEntry);
	static	void			_TransactionIdle(int32 transactionID, int32 event,
								void* _journal);
	static	status_t		_LogFlusher(void* _journal);

private:
			Volume*			fVolume;
			recursive_lock	fLock;
			uint32			fLogSize;
			uint32			fMaxTransactionSize;
			uint32			fUsed;
			mutex			fEntriesLock;
			LogEntryList	fEntries;
			bigtime_t		fTimestamp;

			thread_id		fLogFlusher;
			sem_id			fLogFlusherSem;
			status_t		fInitStatus;

			mutex			fTransactionMapLock;
			BOpenHashTable<TransactionMapHash> fTransactionMap;

			int32			fPendingTransactions[64];
			int32			fPendingTransactionCount;
};


inline uint32
Journal::FreeLogBlocks() const
{
	return fVolume->LogStart() <= fVolume->LogEnd()
		? fLogSize - fVolume->LogEnd() + fVolume->LogStart()
		: fVolume->LogStart() - fVolume->LogEnd();
}


class Transaction {
public:
	friend class Journal;

	Transaction(Volume* volume, off_t refBlock)
		:
		fJournal(NULL),
		fTransactionID(-1)
	{
		Start(volume, refBlock);
	}

	Transaction(Volume* volume, block_run refRun)
		:
		fJournal(NULL),
		fTransactionID(-1)
	{
		Start(volume, volume->ToBlock(refRun));
	}

	Transaction()
		:
		fJournal(NULL),
		fTransactionID(-1)
	{
	}

	~Transaction()
	{
		if (fJournal != NULL)
			fJournal->CommitTransaction(this, false);
	}

	status_t Start(Volume* volume, off_t refBlock);
	bool IsStarted() const { return fJournal != NULL; }

	status_t Done()
	{
		status_t status = B_OK;
		if (fJournal != NULL) {
			status = fJournal->CommitTransaction(this, true);
			if (status == B_OK)
				fJournal = NULL;
		}
		return status;
	}

	bool IsTooLarge() const
	{
		return fJournal->TransactionTooLarge(fTransactionID);
	}

	status_t WriteBlocks(off_t blockNumber, const uint8* buffer,
		size_t numBlocks = 1)
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

	Volume* GetVolume() const
		{ return fJournal != NULL ? fJournal->GetVolume() : NULL; }
	int32 ID() const
		{ return fTransactionID; }

	void AddListener(TransactionListener* listener);
	void RemoveListener(TransactionListener* listener);

	void NotifyListeners(bool success);

private:
	Transaction(const Transaction& other);
	Transaction& operator=(const Transaction& other);
		// no implementation

	Journal*				fJournal;
	TransactionListeners	fListeners;
	int32					fTransactionID;
};


#ifdef BFS_DEBUGGER_COMMANDS
int dump_journal(int argc, char** argv);
#endif


#endif	// JOURNAL_H
