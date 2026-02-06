/*
 * Copyright 2007, Jérôme Duval. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>


/*!
	wait()/waitpid() should return -1 and set errno to ECHILD, since there
	are no children to wait for.
*/
int
main()
{
	int childStatus;
	pid_t pid = wait(&childStatus);
	printf("wait() returned %ld (%s)\n", (long)pid, strerror(errno));
	assert(pid == -1);
	assert(errno == ECHILD);

	pid = waitpid(-1, &childStatus, 0);
	printf("waitpid(-1, ...) returned %ld (%s)\n", (long)pid, strerror(errno));
	assert(pid == -1);
	assert(errno == ECHILD);

	pid = waitpid(0, &childStatus, 0);
	printf("waitpid(0, ...) returned %ld (%s)\n", (long)pid, strerror(errno));
	assert(pid == -1);
	assert(errno == ECHILD);

	pid = waitpid(getpgrp(), &childStatus, 0);
	printf("waitpid(%ld, ...) returned %ld (%s)\n", (long)getpgrp(), (long)pid, strerror(errno));
	assert(pid == -1);
	assert(errno == ECHILD);

	return 0;
}
