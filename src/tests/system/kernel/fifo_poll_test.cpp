#include <errno.h>
#include <stdio.h>
#include <poll.h>
#include <assert.h>
#include <string.h>

int main() {
	/* Test for #7859.
	 * Start a process with popen and watch the pipe using poll().
	 * We should get these events (preferrably in this exact order):
	 * - 3 reads of the values 1, 2 and 3 from the bash script
	 * - 1 end of file event (that should make the pipe readable)
	 * - 1 EINTR return from poll because of the SIGCHLD signal when the child process terminates
	 *
	 * Currently, the EINTR return happens before the end of file event notification.
	 */
	FILE* f = popen("/bin/bash -c 'for i in 1 2 3; do { echo $i; sleep 1; }; done'", "r");
	printf("f=%p\n", f);
	assert(f != NULL);
	int fd = fileno(f);
	printf("fd=%d\n", fd);
	assert(fd >= 0);

	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN | POLLRDBAND;

	char buffer[80];

	while (1) {
		int rv = poll(&pfd, 1, 5000); // Increased timeout to avoid flakiness in VM
		printf("rv=%d\n", rv);
		if (rv == 0)
			continue;
		if (rv < 0) {
			if (errno == EINTR) {
				puts("warning: received SIGCHLD before stream close event");
				continue;
			} else {
				printf("poll returns with error %s\n", strerror(errno));
				break;
			}
		}
		printf("events=%08x revents=%08x\n", pfd.events, pfd.revents);
		assert(rv > 0);
		if ((pfd.events & pfd.revents) == 0)
			break;

		if (fgets(buffer, 79, f) != NULL)
			printf("output: %s", buffer);
	}

	return 0;
}
