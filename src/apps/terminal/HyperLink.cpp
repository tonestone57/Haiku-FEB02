/*
 * Copyright 2013, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */


#include "HyperLink.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "TermConst.h"


HyperLink::HyperLink()
	:
	fAddress(),
	fType(TYPE_URL),
	fOSCRef(0),
	fOSCID(NULL)
{
}


HyperLink::HyperLink(const BString& address, Type type)
	:
	fText(address),
	fAddress(address),
	fType(type),
	fOSCRef(0),
	fOSCID(NULL)
{
}


HyperLink::HyperLink(const BString& text, const BString& address, Type type)
	:
	fText(text),
	fAddress(address),
	fType(type),
	fOSCRef(0),
	fOSCID(NULL)
{
}


HyperLink::HyperLink(const BString& address, uint32 ref, const BString& id)
	:
	fText(NULL),
	fAddress(address),
	fType(TYPE_OSC_URL),
	fOSCRef(ref),
	fOSCID(id)
{
}


status_t
HyperLink::Open()
{
	if (!IsValid())
		return B_BAD_VALUE;

	// open with the "open" program
	const char* args[] = { "/bin/open", fAddress.String(), NULL };

	int status = 0;
	pid_t pid = fork();
	if (pid == 0) {
		execv(args[0], (char* const*)args);
		exit(1);
	} else if (pid > 0) {
		waitpid(pid, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			return B_OK;
		return B_ERROR;
	}

	return errno;
}
