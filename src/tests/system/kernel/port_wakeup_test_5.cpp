/*
 * Copyright 2006, Marcus Overhagen, <marcus@overhagen.de>
 * Distributed under the terms of the MIT License.
 */


#include <OS.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/*
 *
 */

port_id id;
char data[100];

int32
test_thread(void *)
{
	status_t s;

	printf("write port...\n");
	s = write_port(id, 0x5678, data, 20);
	printf("write port result 0x%08lx (%s)\n", s, strerror(s));
	assert(s == B_BAD_PORT_ID);

	return 0;
}


int
main()
{
	status_t s;
	status_t thread_return;
	
	id = create_port(1, "test port");
	printf("created port %ld\n", id);
	assert(id > 0);
	
	s = write_port(id, 0x1234, data, 10);
	printf("write port result 0x%08lx (%s)\n", s, strerror(s));
	assert(s == B_OK);

	printf("write should block for 5 seconds now, as port is full, until port is deleted\n");
	
	thread_id thread = spawn_thread(test_thread, "test thread", B_NORMAL_PRIORITY, NULL);
	assert(thread > 0);
	resume_thread(thread);
	snooze(5000000);

	printf("delete port...\n");
	s = delete_port(id); 
	printf("delete port result 0x%08lx (%s)\n", s, strerror(s));
	assert(s == B_OK);

	printf("waiting for thread to terminate\n");
	s = wait_for_thread(thread, &thread_return);
	assert(s == B_OK);
	assert(thread_return == 0);
	
	return 0;
}
