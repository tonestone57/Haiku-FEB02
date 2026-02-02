/*
 * Copyright 2008-2011, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2002-2008, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */
#ifndef _KERNEL_VM_VM_CACHE_TRACING_H
#define _KERNEL_VM_VM_CACHE_TRACING_H


#include <new>
#include <stddef.h>

#include <tracing.h>
#include <vm/VMArea.h>
#include <vm/VMCache.h>
#include <vm/vm_page.h>


#if VM_CACHE_TRACING

namespace VMCacheTracing {

class VMCacheTraceEntry : public AbstractTraceEntry {
	public:
		VMCacheTraceEntry(VMCache* cache)
			:
			fCache(cache)
		{
#if VM_CACHE_TRACING_STACK_TRACE
			fStackTrace = capture_tracing_stack_trace(
				VM_CACHE_TRACING_STACK_TRACE, 0, true);
				// Don't capture userland stack trace to avoid potential
				// deadlocks.
#endif
		}

#if VM_CACHE_TRACING_STACK_TRACE
		virtual void DumpStackTrace(TraceOutput& out)
		{
			out.PrintStackTrace(fStackTrace);
		}
#endif

		VMCache* Cache() const
		{
			return fCache;
		}

	protected:
		VMCache*	fCache;
#if VM_CACHE_TRACING_STACK_TRACE
		tracing_stack_trace* fStackTrace;
#endif
};


class Create : public VMCacheTraceEntry {
	public:
		Create(VMCache* cache)
			:
			VMCacheTraceEntry(cache)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache create: -> cache: %p", fCache);
		}
};


class Delete : public VMCacheTraceEntry {
	public:
		Delete(VMCache* cache)
			:
			VMCacheTraceEntry(cache)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache delete: cache: %p", fCache);
		}
};


class SetMinimalCommitment : public VMCacheTraceEntry {
	public:
		SetMinimalCommitment(VMCache* cache, off_t commitment)
			:
			VMCacheTraceEntry(cache),
			fOldCommitment(cache->committed_size),
			fCommitment(commitment)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache set min commitment: cache: %p, "
				"commitment: %" B_PRIdOFF " -> %" B_PRIdOFF, fCache,
				fOldCommitment, fCommitment);
		}

	private:
		off_t	fOldCommitment;
		off_t	fCommitment;
};


class Resize : public VMCacheTraceEntry {
	public:
		Resize(VMCache* cache, off_t size)
			:
			VMCacheTraceEntry(cache),
			fOldSize(cache->virtual_end),
			fSize(size)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache resize: cache: %p, size: %" B_PRIdOFF " -> %"
				B_PRIdOFF, fCache, fOldSize, fSize);
		}

	private:
		off_t	fOldSize;
		off_t	fSize;
};


class Rebase : public VMCacheTraceEntry {
	public:
		Rebase(VMCache* cache, off_t base)
			:
			VMCacheTraceEntry(cache),
			fOldBase(cache->virtual_base),
			fBase(base)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache rebase: cache: %p, base: %lld -> %lld", fCache,
				fOldBase, fBase);
		}

	private:
		off_t	fOldBase;
		off_t	fBase;
};


class AddConsumer : public VMCacheTraceEntry {
	public:
		AddConsumer(VMCache* cache, VMCache* consumer)
			:
			VMCacheTraceEntry(cache),
			fConsumer(consumer)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache add consumer: cache: %p, consumer: %p", fCache,
				fConsumer);
		}

		VMCache* Consumer() const
		{
			return fConsumer;
		}

	private:
		VMCache*	fConsumer;
};


class RemoveConsumer : public VMCacheTraceEntry {
	public:
		RemoveConsumer(VMCache* cache, VMCache* consumer)
			:
			VMCacheTraceEntry(cache),
			fConsumer(consumer)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache remove consumer: cache: %p, consumer: %p",
				fCache, fConsumer);
		}

	private:
		VMCache*	fConsumer;
};


class Merge : public VMCacheTraceEntry {
	public:
		Merge(VMCache* cache, VMCache* consumer)
			:
			VMCacheTraceEntry(cache),
			fConsumer(consumer)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache merge with consumer: cache: %p, consumer: %p",
				fCache, fConsumer);
		}

	private:
		VMCache*	fConsumer;
};


class InsertArea : public VMCacheTraceEntry {
	public:
		InsertArea(VMCache* cache, VMArea* area)
			:
			VMCacheTraceEntry(cache),
			fArea(area)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache insert area: cache: %p, area: %p", fCache,
				fArea);
		}

		VMArea*	Area() const
		{
			return fArea;
		}

	private:
		VMArea*	fArea;
};


class RemoveArea : public VMCacheTraceEntry {
	public:
		RemoveArea(VMCache* cache, VMArea* area)
			:
			VMCacheTraceEntry(cache),
			fArea(area)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache remove area: cache: %p, area: %p", fCache,
				fArea);
		}

	private:
		VMArea*	fArea;
};

}	// namespace VMCacheTracing

#	define T(x) new(std::nothrow) VMCacheTracing::x;

#	if VM_CACHE_TRACING >= 2

namespace VMCacheTracing {

class InsertPage : public VMCacheTraceEntry {
	public:
		InsertPage(VMCache* cache, vm_page* page, off_t offset)
			:
			VMCacheTraceEntry(cache),
			fPage(page),
			fOffset(offset)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache insert page: cache: %p, page: %p, offset: %"
				B_PRIdOFF, fCache, fPage, fOffset);
		}

	private:
		vm_page*	fPage;
		off_t		fOffset;
};


class RemovePage : public VMCacheTraceEntry {
	public:
		RemovePage(VMCache* cache, vm_page* page)
			:
			VMCacheTraceEntry(cache),
			fPage(page)
		{
			Initialized();
		}

		virtual void AddDump(TraceOutput& out)
		{
			out.Print("vm cache remove page: cache: %p, page: %p", fCache,
				fPage);
		}

	private:
		vm_page*	fPage;
};

}	// namespace VMCacheTracing

#		define T2(x) new(std::nothrow) VMCacheTracing::x;
#	else
#		define T2(x) ;
#	endif

#else
#	define T(x) ;
#	define T2(x) ;
#endif


#endif	// _KERNEL_VM_VM_CACHE_TRACING_H
