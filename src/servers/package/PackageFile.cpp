/*
 * Copyright 2013-2014, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ingo Weinhold <ingo_weinhold@gmx.de>
 */


#include "PackageFile.h"

#include <fcntl.h>

#include <File.h>
#include <Path.h>

#include <AutoDeleter.h>

#include "AttributeCache.h"
#include "DebugSupport.h"
#include "PackageFileManager.h"


PackageFile::PackageFile()
	:
	fNodeRef(),
	fDirectoryRef(),
	fFileName(),
	fInfo(),
	fEntryRefHashTableNext(NULL),
// 	fNodeRefHashTableNext(NULL),
	fOwner(NULL),
	fIgnoreEntryCreated(0),
	fIgnoreEntryRemoved(0)
{
}


PackageFile::~PackageFile()
{
}


status_t
PackageFile::Init(const entry_ref& entryRef, PackageFileManager* owner)
{
	fDirectoryRef.device = entryRef.device;
	fDirectoryRef.node = entryRef.directory;

	// init the file name
	fFileName = entryRef.name;
	if (fFileName.IsEmpty())
		RETURN_ERROR(B_NO_MEMORY);

	// open the file and get the node_ref
	BFile file;
	status_t error = file.SetTo(&entryRef, B_READ_ONLY);
	if (error != B_OK)
		RETURN_ERROR(error);

	error = file.GetNodeRef(&fNodeRef);
	if (error != B_OK)
		RETURN_ERROR(error);

	// try to load from cache
	bool loadedFromCache = false;
	off_t size;
	time_t mtime;
	BPath path;
	if (file.GetSize(&size) == B_OK && file.GetModificationTime(&mtime) == B_OK
		&& path.SetTo(&entryRef) == B_OK) {
		if (BPrivate::AttributeCache::Load(fInfo, path.Path(), mtime, size) == B_OK)
			loadedFromCache = true;
	}

	if (!loadedFromCache) {
		// get the package info
		FileDescriptorCloser fd(file.Dup());
		if (!fd.IsSet())
			RETURN_ERROR(error);

		error = fInfo.ReadFromPackageFile(fd.Get());
		if (error != B_OK)
			RETURN_ERROR(error);

		// save to cache
		if (path.InitCheck() == B_OK)
			BPrivate::AttributeCache::Save(fInfo, path.Path(), mtime, size);
	}

	if (fFileName != fInfo.CanonicalFileName())
		fInfo.SetFileName(fFileName);

	fOwner = owner;

	return B_OK;
}


BString
PackageFile::RevisionedName() const
{
	return BString().SetToFormat("%s-%s", fInfo.Name().String(),
		fInfo.Version().ToString().String());
}


BString
PackageFile::RevisionedNameThrows() const
{
	BString result(RevisionedName());
	if (result.IsEmpty())
		throw std::bad_alloc();
	return result;
}


void
PackageFile::LastReferenceReleased()
{
	if (fOwner != NULL)
		fOwner->RemovePackageFile(this);
	delete this;
}
