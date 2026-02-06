/*
 * Copyright 2018, Xiang Fan, sfanxiang@gmail.com.
 * Distributed under the terms of the MIT License.
 */

#include <stdio.h>
#include <poll.h>
#include <assert.h>


int main()
{
	FILE* f = fopen("/dev/null", "w");
	printf("f=%p\n", f);
	assert(f != NULL);
	int fd = fileno(f);
	printf("fd=%d\n", fd);
	assert(fd >= 0);

	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLOUT;
	pfd.revents = 0;

	int rv = poll(&pfd, 1, -1);
	printf("rv=%d\n", rv);
	if (rv <= 0)
		return 1;
	printf("events=%08x revents=%08x\n", pfd.events, pfd.revents);
	if (pfd.revents != POLLOUT)
		return 2;

	assert(rv == 1);
	assert(pfd.revents == POLLOUT);

	return 0;
}
