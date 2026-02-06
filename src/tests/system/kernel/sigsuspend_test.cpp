#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <errno.h>


static bool sHandlerCalled = false;

void
handler(int signal)
{
    printf( "inside handler()\n" );
	sHandlerCalled = true;
}


int
main(int argc, char* argv[])
{
	struct sigaction signalAction;
	sigset_t blockedSignalSet;

	sigfillset(&blockedSignalSet);
	sigdelset(&blockedSignalSet, SIGALRM);

	sigemptyset(&signalAction.sa_mask);
	signalAction.sa_flags = 0;
	signalAction.sa_handler = handler;
	int status = sigaction(SIGALRM, &signalAction, NULL);
	assert(status == 0);

    fprintf(stdout, "before sigsuspend()\n");
    alarm(2);
    int result = sigsuspend(&blockedSignalSet);
    fprintf(stdout, "after sigsuspend()\n");

	assert(result == -1);
	assert(errno == EINTR);
	assert(sHandlerCalled);

    return 0;
}
