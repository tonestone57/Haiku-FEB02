/*
 * Copyright 2008-2011, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2002-2008, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */


#include <vm/VMCache.h>

#include <heap.h>
#include <slab/Slab.h>
#include <vm/vm.h>
#include <vm/VMCacheTracing.h>

#include "VMAnonymousCache.h"
#include "VMAnonymousNoSwapCache.h"
#include "VMDeviceCache.h"
#include "VMNullCache.h"
#include "../cache/vnode_store.h"


// #pragma mark - VMCacheFactory


/*static*/ status_t
VMCacheFactory::CreateAnonymousCache(VMCache*& _cache, bool canOvercommit,
	int32 numPrecommittedPages, int32 numGuardPages, bool swappable,
	int priority)
{
	uint32 allocationFlags = HEAP_DONT_WAIT_FOR_MEMORY
		| HEAP_DONT_LOCK_KERNEL_SPACE;
	if (priority >= VM_PRIORITY_VIP)
		allocationFlags |= HEAP_PRIORITY_VIP;

#if ENABLE_SWAP_SUPPORT
	if (swappable) {
		VMAnonymousCache* cache
			= new(gAnonymousCacheObjectCache, allocationFlags) VMAnonymousCache;
		if (cache == NULL)
			return B_NO_MEMORY;

		status_t error = cache->Init(canOvercommit, numPrecommittedPages,
			numGuardPages, allocationFlags);
		if (error != B_OK) {
			cache->Delete();
			return error;
		}

		T(Create(cache));

		_cache = cache;
		return B_OK;
	}
#endif

	VMAnonymousNoSwapCache* cache
		= new(gAnonymousNoSwapCacheObjectCache, allocationFlags)
			VMAnonymousNoSwapCache;
	if (cache == NULL)
		return B_NO_MEMORY;

	status_t error = cache->Init(canOvercommit, numPrecommittedPages,
		numGuardPages, allocationFlags);
	if (error != B_OK) {
		cache->Delete();
		return error;
	}

	T(Create(cache));

	_cache = cache;
	return B_OK;
}


/*static*/ status_t
VMCacheFactory::CreateVnodeCache(VMCache*& _cache, struct vnode* vnode)
{
	const uint32 allocationFlags = HEAP_DONT_WAIT_FOR_MEMORY
		| HEAP_DONT_LOCK_KERNEL_SPACE;
		// Note: Vnode cache creation is never VIP.

	VMVnodeCache* cache
		= new(gVnodeCacheObjectCache, allocationFlags) VMVnodeCache;
	if (cache == NULL)
		return B_NO_MEMORY;

	status_t error = cache->Init(vnode, allocationFlags);
	if (error != B_OK) {
		cache->Delete();
		return error;
	}

	T(Create(cache));

	_cache = cache;
	return B_OK;
}


/*static*/ status_t
VMCacheFactory::CreateDeviceCache(VMCache*& _cache, addr_t baseAddress)
{
	const uint32 allocationFlags = HEAP_DONT_WAIT_FOR_MEMORY
		| HEAP_DONT_LOCK_KERNEL_SPACE;
		// Note: Device cache creation is never VIP.

	VMDeviceCache* cache
		= new(gDeviceCacheObjectCache, allocationFlags) VMDeviceCache;
	if (cache == NULL)
		return B_NO_MEMORY;

	status_t error = cache->Init(baseAddress, allocationFlags);
	if (error != B_OK) {
		cache->Delete();
		return error;
	}

	T(Create(cache));

	_cache = cache;
	return B_OK;
}


/*static*/ status_t
VMCacheFactory::CreateNullCache(int priority, VMCache*& _cache)
{
	uint32 allocationFlags = HEAP_DONT_WAIT_FOR_MEMORY
		| HEAP_DONT_LOCK_KERNEL_SPACE;
	if (priority >= VM_PRIORITY_VIP)
		allocationFlags |= HEAP_PRIORITY_VIP;

	VMNullCache* cache
		= new(gNullCacheObjectCache, allocationFlags) VMNullCache;
	if (cache == NULL)
		return B_NO_MEMORY;

	status_t error = cache->Init(allocationFlags);
	if (error != B_OK) {
		cache->Delete();
		return error;
	}

	T(Create(cache));

	_cache = cache;
	return B_OK;
}
