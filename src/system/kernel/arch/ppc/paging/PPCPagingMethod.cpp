/*
 * Copyright 2010, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */


#include "paging/PPCPagingMethod.h"

#include <vm/VMAddressSpace.h>
#include <vm/VMTranslationMap.h>


PPCPagingMethod* gPPCPagingMethod;


PPCPagingMethod::~PPCPagingMethod()
{
}


bool
PPCPagingMethod::IsKernelPageAccessible(addr_t virtualAddress,
	uint32 protection)
{
	VMAddressSpace* addressSpace = VMAddressSpace::Kernel();
	VMTranslationMap* map = addressSpace->TranslationMap();

	phys_addr_t physicalAddress;
	uint32 flags;
	if (map->Query(virtualAddress, &physicalAddress, &flags) != B_OK)
		return false;

	if ((flags & PAGE_PRESENT) == 0)
		return false;

	// present means kernel-readable, so check for writable
	return (protection & B_KERNEL_WRITE_AREA) == 0
		|| (flags & B_KERNEL_WRITE_AREA) != 0;
}
