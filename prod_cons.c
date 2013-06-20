/******************************************************************************
* FILE: prod_cons.c
* DESCRIPTION:
*  This problem is one of the small collection of standard, well-known problems
*  in concurrent programming: a finite-size buffer and two classes of threads,
*  producers and consumers, put items into the buffer (producers) and take items
*  out of the buffer (consumers).
*
*  A producer must wait until the buffer has space before it can put something
*  in, and a consumer must wait until something is in the buffer before it can
*  take something out.
*
*  A condition variable represents a queue of threads waiting for some condition
*  to be signaled.
*
* ADAPTED FROM:
*  http://docs.oracle.com/cd/E19455-01/806-5257/6je9h032r/index.html
*
* LAST REVISED: 06/20/13  Juri Lelli
******************************************************************************/
#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <assert.h>
#include "rt-app_utils.h"
#include "libcv/dl_syscalls.h"

#define	BSIZE		8
#define NUM_PROD	4
#define NUM_CONS	2

typedef struct {
	char buf[BSIZE];
	int occupied;
	int nextin;
	int nextout;
	pthread_mutex_t mutex;
	pthread_mutexattr_t mutex_attr;
	pthread_cond_t more;
	pthread_cond_t less;
} buffer_t;

buffer_t buffer;
int trace_fd = -1;
int marker_fd = -1;
int pi_cv_enabled = 0;

void *producer(void *d)
{
	char item = 'a';
	buffer_t *b = &buffer;

	pthread_mutex_lock(&b->mutex);

	while (b->occupied >= BSIZE)
		pthread_cond_wait(&b->less, &b->mutex);

	assert(b->occupied < BSIZE);

	b->buf[b->nextin++] = item;

	b->nextin %= BSIZE;
	b->occupied++;

	/*
	 * now: either b->occupied < BSIZE and b->nextin is the index
	 * of the next empty slot in the buffer, or
	 * b->occupied == BSIZE and b->nextin is the index of the
	 * next (occupied) slot that will be emptied by a consumer
	 * (such as b->nextin == b->nextout)
	 */

	pthread_cond_signal(&b->more);

	pthread_mutex_unlock(&b->mutex);
}

void *consumer(void *d)
{
	char item;
	buffer_t *b = &buffer;
	pthread_mutex_lock(&b->mutex);
	while(b->occupied <= 0)
		pthread_cond_wait(&b->more, &b->mutex);

	assert(b->occupied > 0);

	item = b->buf[b->nextout++];
	b->nextout %= BSIZE;
	b->occupied--;

	/*
	 * now: either b->occupied > 0 and b->nextout is the index
	 * of the next occupied slot in the buffer, or
	 * b->occupied == 0 and b->nextout is the index of the next
	 * (empty) slot that will be filled by a producer (such as
	 * b->nextout == b->nextin)
	 */

	pthread_cond_signal(&b->less);
	pthread_mutex_unlock(&b->mutex);
}

int main(int argc, char *argv[])
{
	int i, rc, ret; 
	pthread_t threads[NUM_PROD + NUM_CONS];
	pthread_attr_t attr;
	struct sched_param param;
	cpu_set_t mask;
	char *debugfs;
	char path[256];
	
	if (argc > 1)
		pi_cv_enabled = atoi(argv[1]);

	debugfs = "/debug";
	strcpy(path, debugfs);
	strcat(path,"/tracing/tracing_on");
	trace_fd = open(path, O_WRONLY);
	if (trace_fd >= 0)
	        write(trace_fd, "1", 1);

	strcpy(path, debugfs);
	strcat(path,"/tracing/trace_marker");
	marker_fd = open(path, O_WRONLY);
	
	CPU_ZERO(&mask);
	CPU_SET(1, &mask);
	ret = sched_setaffinity(0, sizeof(mask), &mask);
	if (ret != 0) {
		printf("pthread_setaffinity failed\n"); 
		exit(EXIT_FAILURE);
	}
	
	param.sched_priority = 96;
	ret = pthread_setschedparam(pthread_self(), 
				    SCHED_FIFO, 
				    &param);
	if (ret != 0) {
		printf("pthread_setschedparam failed\n"); 
		exit(EXIT_FAILURE);
	}
	
	/* Initialize mutex and condition variable objects */
	pthread_mutexattr_init(&buffer.mutex_attr);
	pthread_mutexattr_setprotocol(&buffer.mutex_attr, PTHREAD_PRIO_INHERIT);
	pthread_mutex_init(&buffer.mutex, &buffer.mutex_attr);
	pthread_cond_init (&buffer.more, NULL);
	pthread_cond_init (&buffer.less, NULL);
	
	/* For portability, explicitly create threads in a joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	for (i = 0; i < NUM_CONS; i++) {
		//ftrace_write(marker_fd, "[main]: creating consumer()\n");
		pthread_create(&threads[i], &attr, consumer, NULL);
	}

	for (i = NUM_CONS; i < (NUM_CONS + NUM_PROD); i++) {
		//ftrace_write(marker_fd, "[main]: creating producer()\n");
		pthread_create(&threads[i], &attr, producer, NULL);
	}
	
	/* Wait for all threads to complete */
	for (i = 0; i < (NUM_CONS + NUM_PROD); i++) {
		pthread_join(threads[i], NULL);
	}
	printf ("Main(): Waited and joined with %d threads. Done.\n", 
		NUM_CONS + NUM_PROD);
	
	if (trace_fd >= 0)
	        write(trace_fd, "0", 1);

	/* Clean up and exit */
	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&buffer.mutex);
	pthread_cond_destroy(&buffer.more);
	pthread_cond_destroy(&buffer.less);
	pthread_exit (NULL);
}
