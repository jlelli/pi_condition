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
#include <sys/types.h>
#include <signal.h>
#include "rt-app_utils.h"
#include "libcv/dl_syscalls.h"

#define	BSIZE		8
#define NUM_PROD	2
#define NUM_CONS	1
#define	DURATION	10

typedef struct {
	int buf[BSIZE];
	int occupied;
	int nextin;
	int nextout;
	pthread_mutex_t mutex;
	pthread_mutexattr_t mutex_attr;
	pthread_cond_t more;
	pthread_cond_t less;
} buffer_t;

buffer_t buffer;
pid_t pids[NUM_PROD + NUM_CONS];
int trace_fd = -1;
int marker_fd = -1;
int pi_cv_enabled = 0;
volatile int shutdown = 0;

static inline busywait(struct timespec *to)
{
	struct timespec t_step;
	while (1) {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_step);
		if (!timespec_lower(&t_step, to))
			break;
	}
}

void *producer(void *d)
{
	int ret;
	struct sched_param param;
	long id = (long) d;
	int item = id;
	buffer_t *b = &buffer;
	struct timespec twait, now;
	cpu_set_t mask;
	pid_t my_pid = gettid();

	pids[id] = my_pid;
	
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	ret = sched_setaffinity(0, sizeof(mask), &mask);
	if (ret != 0) {
		printf("pthread_setaffinity failed\n"); 
		exit(EXIT_FAILURE);
	}
	
	param.sched_priority = 92;
	ret = pthread_setschedparam(pthread_self(), 
				    SCHED_FIFO, 
				    &param);
	if (ret != 0) {
		printf("pthread_setschedparam failed\n"); 
		exit(EXIT_FAILURE);
	}

	if (pi_cv_enabled) {
		ftrace_write(marker_fd, "Adding helper thread: pid %d,"
			     " prio 92\n", my_pid);
		pthread_cond_helpers_add(&buffer.more, my_pid);
		ftrace_write(marker_fd, "[prod %d] helps on cv %p\n",
			     my_pid, &buffer.more);
	}

	while(!shutdown) {
		pthread_mutex_lock(&b->mutex);

		while (b->occupied >= BSIZE)
			pthread_cond_wait(&b->less, &b->mutex);

		assert(b->occupied < BSIZE);

		b->buf[b->nextin++] = item;
		twait = usec_to_timespec(600000L);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
		twait = timespec_add(&now, &twait);
		busywait(&twait);
		ftrace_write(marker_fd, "[prod %d] produced %d\n", my_pid, item);

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
		sleep(1);
	}

	if (pi_cv_enabled) {
		pthread_cond_helpers_del(&buffer.more, my_pid);
		ftrace_write(marker_fd, "[prod %d] stop helping on cv %p\n",
			     my_pid, &buffer.more);
		ftrace_write(marker_fd, "Removing helper thread: pid %d,"
			     " prio 92\n", my_pid);
	}
}

void *consumer(void *d)
{
	int ret;
	struct sched_param param;
	long id = (long) d;
	int item;
	buffer_t *b = &buffer;
	struct timespec twait, now;
	cpu_set_t mask;
	pid_t my_pid = gettid();

	pids[id] = my_pid;
	
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	ret = sched_setaffinity(0, sizeof(mask), &mask);
	if (ret != 0) {
		printf("pthread_setaffinity failed\n"); 
		exit(EXIT_FAILURE);
	}
	
	param.sched_priority = 94;
	ret = pthread_setschedparam(pthread_self(), 
				    SCHED_FIFO, 
				    &param);
	if (ret != 0) {
		printf("pthread_setschedparam failed\n"); 
		exit(EXIT_FAILURE);
	}
	
	/**
	 * Give producers some time to set up.
	 */
	sleep(1);

	while(!shutdown) {
		pthread_mutex_lock(&b->mutex);
		while(b->occupied <= 0) {
			ftrace_write(marker_fd, "[cons %d] waits\n",
				     my_pid);
			pthread_cond_wait(&b->more, &b->mutex);
		}
	
		assert(b->occupied > 0);
	
		item = b->buf[b->nextout++];
		twait = usec_to_timespec(300000L);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
		twait = timespec_add(&now, &twait);
		busywait(&twait);
		ftrace_write(marker_fd, "[cons %d] consumed %d\n", my_pid, item);

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
}

void *annoyer(void *d)
{
	int ret;
	long id = (long) d;
	struct timespec twait, now;
	struct sched_param param;
	cpu_set_t mask;
	pid_t my_pid = gettid();

	pids[id] = my_pid;
	
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	ret = sched_setaffinity(0, sizeof(mask), &mask);
	if (ret != 0) {
		printf("pthread_setaffinity failed\n"); 
		exit(EXIT_FAILURE);
	}
	
	param.sched_priority = 93;
	ret = pthread_setschedparam(pthread_self(), 
				    SCHED_FIFO, 
				    &param);
	if (ret != 0) {
		printf("pthread_setschedparam failed\n"); 
		exit(EXIT_FAILURE);
	}

	/**
	 * Give other some time to warm up.
	 */
	sleep(2);

	ftrace_write(marker_fd, "Starting annoyer(): prio 93\n");

	while(1) {
		twait = usec_to_timespec(200000L);
		ftrace_write(marker_fd, "[annoyer %d] starts running...\n", my_pid);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
		twait = timespec_add(&now, &twait);
		busywait(&twait);
		ftrace_write(marker_fd, "[annoyer %d] sleeps.\n", my_pid);
		sleep(1);
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int i, ret; 
	long id = 0;
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
		ftrace_write(marker_fd, "[main]: creating consumer()\n");
		pthread_create(&threads[i], &attr, consumer, (void *)id);
		id++;
	}

	for (i = NUM_CONS; i < (NUM_CONS + NUM_PROD); i++) {
		ftrace_write(marker_fd, "[main]: creating producer()\n");
		pthread_create(&threads[i], &attr, producer, (void *)id);
		id++;
	}

	ftrace_write(marker_fd, "[main]: creating annoyer()\n");
	pthread_create(&threads[i], &attr, annoyer, (void *)id);

	sleep(DURATION);
	shutdown = 1;
	for (i = 0; i < (NUM_CONS + NUM_PROD); i++) {
		kill(pids[i], 9);
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
