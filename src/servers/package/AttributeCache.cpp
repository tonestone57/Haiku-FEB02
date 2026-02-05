/*
 * Copyright 2024, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */


#include "AttributeCache.h"

#include <new>
#include <stdio.h>
#include <sys/stat.h>

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <SHA256.h>
#include <String.h>

#include <package/PackageInfo.h>

#include "DebugSupport.h"


namespace BPackageKit {
namespace BPrivate {


static const uint32 kCacheMagic = 'PkAc';
static const uint32 kCacheVersion = 1;


struct CacheHeader {
	uint32 magic;
	uint32 version;
	time_t mtime;
	off_t size;
};


static status_t
GetCachePath(const char* packagePath, BPath& _path)
{
	BPath cacheDir;
	status_t error = find_directory(B_SYSTEM_CACHE_DIRECTORY, &cacheDir);
	if (error != B_OK)
		return error;

	error = cacheDir.Append("package_daemon/attributes");
	if (error != B_OK)
		return error;

	SHA256 sha;
	sha.Init();
	sha.Update(packagePath, strlen(packagePath));
	const uint8* digest = sha.Digest();

	BString filename;
	for (size_t i = 0; i < sha.DigestLength(); i++) {
		char buffer[3];
		snprintf(buffer, sizeof(buffer), "%02x", digest[i]);
		filename << buffer;
	}

	BString leaf(packagePath);
	int32 lastSlash = leaf.FindLast('/');
	if (lastSlash >= 0)
		leaf.Remove(0, lastSlash + 1);

	filename << "_" << leaf;
	filename.Truncate(250);
	filename << ".info";

	error = _path.SetTo(cacheDir.Path(), filename.String());
	return error;
}


status_t
AttributeCache::Load(BPackageInfo& info, const char* packagePath, time_t mtime,
	off_t size)
{
	BPath path;
	status_t error = GetCachePath(packagePath, path);
	if (error != B_OK)
		return error;

	BFile file(path.Path(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();

	CacheHeader header;
	ssize_t bytesRead = file.Read(&header, sizeof(header));
	if (bytesRead != sizeof(header))
		return B_IO_ERROR;

	if (header.magic != kCacheMagic || header.version != kCacheVersion)
		return B_BAD_VALUE;

	if (header.mtime != mtime || header.size != size)
		return B_MISMATCHED_VALUES;

	BMessage archive;
	error = archive.Unflatten(&file);
	if (error != B_OK)
		return error;

	info.~BPackageInfo();
	new (&info) BPackageInfo(&archive, &error);

	return error;
}


status_t
AttributeCache::Save(const BPackageInfo& info, const char* packagePath,
	time_t mtime, off_t size)
{
	BPath path;
	status_t error = GetCachePath(packagePath, path);
	if (error != B_OK)
		return error;

	BPath parent;
	path.GetParent(&parent);
	create_directory(parent.Path(), 0755);

	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();

	CacheHeader header;
	header.magic = kCacheMagic;
	header.version = kCacheVersion;
	header.mtime = mtime;
	header.size = size;

	ssize_t bytesWritten = file.Write(&header, sizeof(header));
	if (bytesWritten != sizeof(header))
		return B_IO_ERROR;

	BMessage archive;
	error = info.Archive(&archive);
	if (error != B_OK)
		return error;

	return archive.Flatten(&file);
}


}	// namespace BPrivate
}	// namespace BPackageKit
