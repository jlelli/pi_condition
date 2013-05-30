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

#define NUM_THREADS 5 

int count = 0;
int trace_fd = -1;
int marker_fd = -1;
int pi_cv_enabled = 0;
int watch_prio = 93;
pthread_mutex_t count_mutex;
pthread_mutexattr_t count_mutex_attr;
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

	if (pi_cv_enabled) {
		ftrace_write(marker_fd, "Adding helper thread: pid %d, prio 93\n", my_pid);
		pthread_cond_helpers_add(&count_threshold_cv, my_pid);
		ftrace_write(marker_fd, "helps on cv %p\n", &count_threshold_cv);
	}

	ftrace_write(marker_fd, "Starting inc_count(): pid %d, prio 93\n", my_pid);
	printf("Starting inc_count(): pid %d, prio 93\n", my_pid);

	while (count < 2) {
		sleep(1);
	}	

	pthread_mutex_lock(&count_mutex);

	/* Do some work (e.g., fill up the queue) */
	twait = usec_to_timespec(6000000L);
	busywait(&twait);
	count++;
	
	ftrace_write(marker_fd, "signals on cv %p\n", &count_threshold_cv);
	printf("[inc_count] signals on cv %p\n", &count_threshold_cv);
	pthread_cond_broadcast(&count_threshold_cv);
	ftrace_write(marker_fd, "Just sent signal.\n");
	ftrace_write(marker_fd, "inc_count(): pid %d, unlocking mutex\n", my_pid);
	pthread_mutex_unlock(&count_mutex);
	

	if (pi_cv_enabled) {
		pthread_cond_helpers_del(&count_threshold_cv, my_pid);
		ftrace_write(marker_fd, "stop helping on cv %p\n", &count_threshold_cv);
		ftrace_write(marker_fd, "Removing helper thread: pid %d, prio 93\n", my_pid);
	}
	pthread_exit(NULL);
}

void *watch_count(void *t) 
{
	int ret;
	pid_t my_pid = gettid();
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

	ftrace_write(marker_fd, "Starting watch_count(): pid %d, prio %d\n", my_pid, watch_prio);
	printf("Starting watch_count(): pid %d, prio %d\n", my_pid, watch_prio);
	param.sched_priority = watch_prio++;
	ret = pthread_setschedparam(pthread_self(), 
				    SCHED_FIFO, 
				    &param);
	if (ret != 0) {
		printf("pthread_setschedparam failed\n"); 
		exit(EXIT_FAILURE);
	}

	twait = usec_to_timespec(100000L);
	busywait(&twait);
	
	/*
	Lock mutex and wait for signal.  Note that the pthread_cond_wait routine
	will automatically and atomically unlock mutex while it waits. 
	*/
	pthread_mutex_lock(&count_mutex);
	ftrace_write(marker_fd, "watch_count(): Going into wait...\n");
	ftrace_write(marker_fd, "waits on cv %p\n", &count_threshold_cv);
	printf("[watch_count] %d waits on cv %p\n", my_pid, &count_threshold_cv);
	count++;
	pthread_cond_wait(&count_threshold_cv, &count_mutex);
	ftrace_write(marker_fd, "wakes on cv %p\n", &count_threshold_cv);
	printf("[watch_count] %d wakes on cv %p\n", my_pid, &count_threshold_cv);
	/* "Consume" the item... */
	ftrace_write(marker_fd, "watch_count(): pid %d, Condition signal received.\n", my_pid);
	printf("[watch_count] pid %d, Condition signal received.\n", my_pid);
	ftrace_write(marker_fd, "watch_count(): pid %d, Consuming an item...\n", my_pid);
	twait = usec_to_timespec(2000000L);
	busywait(&twait);
	
	ftrace_write(marker_fd, "watch_count(): pid %d, Unlocking mutex.\n", my_pid);
	pthread_mutex_unlock(&count_mutex);

	pthread_exit(NULL);
}

void *annoyer(void *t)
{
	int ret;
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
	
	param.sched_priority = 93;
	ret = pthread_setschedparam(pthread_self(), 
				    SCHED_FIFO, 
				    &param);
	if (ret != 0) {
		printf("pthread_setschedparam failed\n"); 
		exit(EXIT_FAILURE);
	}
	ftrace_write(marker_fd, "Starting annoyer(): prio 93\n");

	ftrace_write(marker_fd, "annoyer thread should preempt inc_count for 5sec\n");

	twait = usec_to_timespec(5000000L);
	ftrace_write(marker_fd, "starts running...\n");
	busywait(&twait);

	ftrace_write(marker_fd, "annoyer thread dies...\n");
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int i, rc, ret; 
	pthread_t threads[NUM_THREADS];
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
	pthread_mutexattr_init(&count_mutex_attr);
	pthread_mutexattr_setprotocol(&count_mutex_attr, PTHREAD_PRIO_INHERIT);
	pthread_mutex_init(&count_mutex, &count_mutex_attr);
	pthread_cond_init (&count_threshold_cv, NULL);
	
	/* For portability, explicitly create threads in a joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	ftrace_write(marker_fd, "[main]: creating inc_count\n");
	pthread_create(&threads[1], &attr, inc_count, NULL);
	sleep(1);
	ftrace_write(marker_fd, "[main]: creating watch_count\n");
	pthread_create(&threads[0], &attr, watch_count, NULL);
	ftrace_write(marker_fd, "[main]: creating watch_count\n");
	pthread_create(&threads[3], &attr, watch_count, NULL);
	ftrace_write(marker_fd, "[main]: creating watch_count\n");
	pthread_create(&threads[4], &attr, watch_count, NULL);
	sleep(5);
	ftrace_write(marker_fd, "[main]: creating annoyer\n");
	pthread_create(&threads[2], &attr, annoyer, NULL);
	
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
