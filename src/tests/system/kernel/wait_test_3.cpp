/*
 * Copyright 2007, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>


/*!
	waitpid() should wait only once.
*/


int
child2()
{
	printf("child 2 1. parent id = %ld\n", (long)getppid());
	sleep(2);
	printf("child 2 2. parent id = %ld\n", (long)getppid());
	return 2;
}


//! exits before child 2
int
child1()
{
	printf("child 1 process group: %ld\n", (long)getpgrp());

	pid_t child = fork();
	if (child == 0)
		return child2();

	sleep(1);
	return 1;
}


int
main()
{
	printf("main process group: %ld\n", (long)getpgrp());
	pid_t child = fork();
	if (child == 0)
		return child1();

	pid_t pid;
	int count = 0;
	do {
		int childStatus = -1;
		pid = waitpid(0, &childStatus, 0);
		printf("waitpid() returned %ld (%s), child status %d\n", (long)pid, strerror(errno), childStatus);
		if (pid >= 0) {
			assert(pid == child);
			count++;
		}
	} while (pid >= 0);

	assert(count == 1);
	assert(errno == ECHILD);

	return 0;
}
