/*
 * Copyright 2001-2012, Axel Dörfler, axeld@pinc-software.de.
 * This file may be used under the terms of the MIT License.
 */
#ifndef JOURNAL_H
#define JOURNAL_H


#include "system_dependencies.h"

#include "Volume.h"
#include "Utility.h"


struct run_array;
class Inode;
class LogEntry;
class Journal;
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


class Transaction {
public:
	Transaction(Volume* volume, off_t refBlock)
		:
		fJournal(NULL),
		fParent(NULL),
		fTransactionID(-1)
	{
		Start(volume, refBlock);
	}

	Transaction(Volume* volume, block_run refRun)
		:
		fJournal(NULL),
		fParent(NULL),
		fTransactionID(-1)
	{
		Start(volume, volume->ToBlock(refRun));
	}

	Transaction()
		:
		fJournal(NULL),
		fParent(NULL),
		fTransactionID(-1)
	{
	}

	~Transaction();

	status_t Start(Volume* volume, off_t refBlock);
	bool IsStarted() const { return fJournal != NULL; }

	status_t Done();

	bool HasParent() const
	{
		return fParent != NULL;
	}

	bool IsTooLarge() const;

	status_t WriteBlocks(off_t blockNumber, const uint8* buffer,
		size_t numBlocks = 1);

	Volume* GetVolume() const;
	int32 ID() const
		{ return fTransactionID; }

	void AddListener(TransactionListener* listener);
	void RemoveListener(TransactionListener* listener);

	void NotifyListeners(bool success);
	void MoveListenersTo(Transaction* transaction);

	Transaction* Parent() const
		{ return fParent; }

	thread_id Thread() const
		{ return fThread; }

private:
	friend class Journal;

	status_t Unlock(bool success = false);

	Transaction(const Transaction& other);
	Transaction& operator=(const Transaction& other);
		// no implementation

	Journal*				fJournal;
	TransactionListeners	fListeners;
	Transaction*			fParent;
	int32					fTransactionID;
	thread_id				fThread;
	DoublyLinkedListLink<Transaction> fActiveLink;
};


class Journal {
public:
							Journal(Volume* volume);
							~Journal();

			status_t		InitCheck();

			status_t		Lock(Transaction* owner);
			status_t		Unlock(Transaction* owner, bool success);

			status_t		ReplayLog();

			size_t			CurrentTransactionSize(int32 transactionID) const;
			bool			CurrentTransactionTooLarge(int32 transactionID) const;

			status_t		FlushLogAndBlocks();
			status_t		FlushLogAndLockJournal();

			Volume*			GetVolume() const { return fVolume; }

	inline	uint32			FreeLogBlocks() const;

			status_t		MoveLog(block_run newLog);

#ifdef BFS_DEBUGGER_COMMANDS
			void			Dump();
#endif

private:
			status_t		_FlushLog(bool flushBlocks);
			uint32			_TransactionSize(int32 transactionID) const;
			status_t		_WriteTransactionToLog(int32 transactionID,
								bool* _transactionEnded = NULL);
			status_t		_CheckRunArray(const run_array* array);
			status_t		_ReplayRunArray(off_t* start);

	static	void			_TransactionWritten(int32 transactionID,
								int32 event, void* _logEntry);

private:
			Volume*			fVolume;
			rw_lock			fTransactionLock;
			recursive_lock	fLogLock;
			thread_id		fOwner;
			uint32			fLogSize;
			uint32			fMaxTransactionSize;
			uint32			fUsed;
			mutex			fEntriesLock;
			LogEntryList	fEntries;
			DoublyLinkedList<Transaction,
				DoublyLinkedListMemberGetLink<Transaction,
					&Transaction::fActiveLink> > fActiveTransactions;
			bigtime_t		fTimestamp;
};


inline uint32
Journal::FreeLogBlocks() const
{
	return fVolume->LogStart() <= fVolume->LogEnd()
		? fLogSize - fVolume->LogEnd() + fVolume->LogStart()
		: fVolume->LogStart() - fVolume->LogEnd();
}


// Inline implementation of Transaction methods

inline status_t
Transaction::Unlock(bool success)
{
	status_t status = fJournal->Unlock(this, success);
	fJournal = NULL;
	fTransactionID = -1;
	return status;
}

inline Transaction::~Transaction()
{
	if (fJournal != NULL)
		Unlock();
}

inline status_t
Transaction::Done()
{
	if (fJournal != NULL)
		return Unlock(true);
	return B_OK;
}

inline bool
Transaction::IsTooLarge() const
{
	return fJournal->CurrentTransactionTooLarge(ID());
}

inline Volume*
Transaction::GetVolume() const
{
	return fJournal != NULL ? fJournal->GetVolume() : NULL;
}


#ifdef BFS_DEBUGGER_COMMANDS
int dump_journal(int argc, char** argv);
#endif


#endif	// JOURNAL_H
