/*
 * Copyright 2024, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef ATTRIBUTE_CACHE_H
#define ATTRIBUTE_CACHE_H


#include <SupportDefs.h>
#include <time.h>


namespace BPackageKit {

class BPackageInfo;

namespace BPrivate {


class AttributeCache {
public:
	static status_t Load(BPackageInfo& info, const char* packagePath,
		time_t mtime, off_t size);
	static status_t Save(const BPackageInfo& info, const char* packagePath,
		time_t mtime, off_t size);
};


}	// namespace BPrivate
}	// namespace BPackageKit


#endif	// ATTRIBUTE_CACHE_H
