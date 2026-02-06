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
	wait() should wait only once. If any argument is given, waitpid() should return
	an error (and errno to ECHILD), since there is no child with that process group ID.
*/


int
child2()
{
	sleep(2);
	return 2;
}


//! exits before child 2
int
child1()
{
	setpgrp();
		// put us into a new process group

	pid_t child = fork();
	if (child == 0)
		return child2();

	sleep(1);
	return 1;
}


int
main(int argc, char** argv)
{
	bool waitForGroup = argc > 1;

	pid_t child = fork();
	if (child == 0)
		return child1();

	pid_t pid;
	int count = 0;
	do {
		int childStatus = -1;
		if (waitForGroup)
			pid = waitpid(0, &childStatus, 0);
		else
			pid = wait(&childStatus);
		printf("wait() returned %ld (%s), child status %d\n",
			(long)pid, strerror(errno), childStatus);

		if (pid >= 0) {
			assert(pid == child);
			count++;
		}
	} while (pid >= 0);

	assert(count == 1);
	assert(errno == ECHILD);

	return 0;
}
