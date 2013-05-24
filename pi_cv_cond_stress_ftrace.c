/******************************************************************************
* FILE: pi_cv_cond_stress.c
* DESCRIPTION:
*   Example code for demonstrating how the use of pi condition variables can
*   ameliorate priority inversion problems.
*
* LAST REVISED: 05/22/13  Juri Lelli
******************************************************************************/
#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include "rt-app_utils.h"
#include "libcv/dl_syscalls.h"

#define NUM_THREADS  3

int count = 0;
int trace_fd = -1;
int marker_fd = -1;
pthread_mutex_t count_mutex;
pthread_cond_t count_threshold_cv;

static inline busywait(struct timespec *to)
{
	struct timespec t_step;
	while (1) {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_step);
		if (!timespec_lower(&t_step, to))
			break;
	}
}

void *inc_count(void *t) 
{
	int i, ret;
	long my_id = (long)t;
	struct timespec twait;
	struct sched_param param;
	cpu_set_t mask;
	pid_t my_pid = gettid();
	
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

	printf("Adding helper thread: thread %ld prio 93 pid %d\n", my_id, my_pid);
	ftrace_write(marker_fd, "helps on cv %p\n", &count_threshold_cv);
	pthread_cond_helpers_add(&count_threshold_cv, my_pid);
	sleep(2);

	printf("Starting inc_count(): thread %ld prio 93\n", my_id);
	
	pthread_mutex_lock(&count_mutex);

	/* Do some work (e.g., fill up the queue) */
	twait = usec_to_timespec(6000000L);
	busywait(&twait);
	count++;
	
	printf("inc_count(): thread %ld, count = %d\n",
	       my_id, count);
	ftrace_write(marker_fd, "signals on cv %p\n", &count_threshold_cv);
	pthread_cond_signal(&count_threshold_cv);
	printf("Just sent signal.\n");
	printf("inc_count(): thread %ld, count = %d, unlocking mutex\n", 
	       my_id, count);
	pthread_mutex_unlock(&count_mutex);

	pthread_cond_helpers_del(&count_threshold_cv, my_pid);
	ftrace_write(marker_fd, "stop helping on cv %p\n", &count_threshold_cv);
	printf("Removing helper thread: thread %ld prio 93 pid %d\n", my_id, my_pid);
	pthread_exit(NULL);
}

void *watch_count(void *t) 
{
	int ret;
	long my_id = (long)t;
	struct timespec twait;
	struct sched_param param;
	cpu_set_t mask;
	
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	ret = sched_setaffinity(0, sizeof(mask), &mask);
	if (ret != 0) {
		printf("pthread_setaffinity failed\n"); 
		exit(EXIT_FAILURE);
	}

	param.sched_priority = 95;
	ret = pthread_setschedparam(pthread_self(), 
				    SCHED_FIFO, 
				    &param);
	if (ret != 0) {
		printf("pthread_setschedparam failed\n"); 
		exit(EXIT_FAILURE);
	}
	printf("Starting watch_count(): thread %ld prio 95\n", my_id);
	twait = usec_to_timespec(500000L);
	busywait(&twait);
	
	/*
	Lock mutex and wait for signal.  Note that the pthread_cond_wait routine
	will automatically and atomically unlock mutex while it waits. 
	*/
	pthread_mutex_lock(&count_mutex);
	printf("watch_count(): thread %ld Count= %d. Going into wait...\n", my_id,count);
	ftrace_write(marker_fd, "waits on cv %p\n", &count_threshold_cv);
	pthread_cond_wait(&count_threshold_cv, &count_mutex);
	ftrace_write(marker_fd, "wakes on cv %p\n", &count_threshold_cv);
	/* "Consume" the item... */
	printf("watch_count(): thread %ld Condition signal received. Count= %d\n", my_id,count);
	printf("watch_count(): thread %ld Consuming an item...\n", my_id,count);
	twait = usec_to_timespec(2000000L);
	busywait(&twait);
	count -= 1;
	printf("watch_count(): thread %ld count now = %d.\n", my_id, count);
	
	printf("watch_count(): thread %ld Unlocking mutex.\n", my_id);
	pthread_mutex_unlock(&count_mutex);

	pthread_exit(NULL);
}

void *annoyer(void *t)
{
	int ret;
	long my_id = (long)t;
	struct timespec twait;
	struct sched_param param;
	cpu_set_t mask;
	
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
	printf("Starting annoyer(): thread %ld prio 94\n", my_id);

	printf("annoyer thread should preempt inc_count for 5sec\n");

	twait = usec_to_timespec(5000000L);
	ftrace_write(marker_fd, "starts running...\n");
	busywait(&twait);

	printf("annoyer thread dies... inc_count can resume\n");
	ftrace_write(marker_fd, "dies\n");
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int i, rc, ret; 
	long t1=1, t2=2, t3=3;
	pthread_t threads[3];
	pthread_attr_t attr;
	struct sched_param param;
	cpu_set_t mask;
	char *debugfs;
	char path[256];

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
	pthread_mutex_init(&count_mutex, NULL);
	pthread_cond_init (&count_threshold_cv, NULL);
	
	/* For portability, explicitly create threads in a joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	ftrace_write(marker_fd, "[main]: creating inc_count\n");
	pthread_create(&threads[1], &attr, inc_count, (void *)t2);
	sleep(1);
	ftrace_write(marker_fd, "[main]: creating watch_count\n");
	pthread_create(&threads[0], &attr, watch_count, (void *)t1);
	sleep(3);
	ftrace_write(marker_fd, "[main]: creating annoyer\n");
	pthread_create(&threads[2], &attr, annoyer, (void *)t3);
	
	/* Wait for all threads to complete */
	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
	printf ("Main(): Waited and joined with %d threads. Final value of count = %d. Done.\n", 
	        NUM_THREADS, count);
	
	if (trace_fd >= 0)
	        write(trace_fd, "0", 1);
	/* Clean up and exit */
	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&count_mutex);
	pthread_cond_destroy(&count_threshold_cv);
	pthread_exit (NULL);
}
