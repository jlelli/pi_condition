/******************************************************************************
* FILE: pi_cv_mutex_chain.c
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

#define NUM_THREADS  4

int trace_fd = -1;
int marker_fd = -1;
int pi_cv_enabled = 0;
pthread_mutex_t count_mutex;
pthread_mutexattr_t count_mutex_attr;
pthread_cond_t count_threshold_cv;
pthread_mutex_t rt_mutex;
pthread_mutexattr_t rt_mutex_attr;

static inline busywait(struct timespec *to)
{
	struct timespec t_step;
	while (1) {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_step);
		if (!timespec_lower(&t_step, to))
			break;
	}
}

void *rt_owner(void *d) 
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

	param.sched_priority = 92;
	ret = pthread_setschedparam(pthread_self(), 
				    SCHED_FIFO, 
				    &param);
	if (ret != 0) {
		printf("pthread_setschedparam failed\n"); 
		exit(EXIT_FAILURE);
	}

	ftrace_write(marker_fd, "Starting rt_owner(): pid %d prio 92\n", my_pid);
	
	pthread_mutex_lock(&rt_mutex);

	/* Do some work (e.g., fill up the queue) */
	twait = usec_to_timespec(6000000L);
	busywait(&twait);
	
	ftrace_write(marker_fd, "rt_owner(): pid %d, unlocking mutex\n", my_pid);
	pthread_mutex_unlock(&rt_mutex);

	pthread_exit(NULL);
}

void *helper(void *d) 
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
		ftrace_write(marker_fd, "Adding helper() thread: pid %d prio 93\n", my_pid);
		pthread_cond_helpers_add(&count_threshold_cv, my_pid);
		ftrace_write(marker_fd, "helper(): helps on cv %p\n", &count_threshold_cv);
	}

	sleep(1);
	ftrace_write(marker_fd, "Starting helper(): pid %d prio 93\n", my_pid);
	
	pthread_mutex_lock(&count_mutex);

	/* Do some work (e.g., fill up the queue) */
	twait = usec_to_timespec(3000000L);
	busywait(&twait);
	
	/* Then block on an rt_mutex */
	ftrace_write(marker_fd, "helper() blocks on rt_mutex %p\n", &rt_mutex);
	pthread_mutex_lock(&rt_mutex);
	twait = usec_to_timespec(3000000L);
	busywait(&twait);
	pthread_mutex_unlock(&rt_mutex);
	
	ftrace_write(marker_fd, "helper() signals on cv %p\n", &count_threshold_cv);
	pthread_cond_broadcast(&count_threshold_cv);
	ftrace_write(marker_fd, "helper(): just sent signal.\n");
	ftrace_write(marker_fd, "helper(): pid %d, unlocking mutex\n", my_pid);
	pthread_mutex_unlock(&count_mutex);

	if (pi_cv_enabled) {
		pthread_cond_helpers_del(&count_threshold_cv, my_pid);
		ftrace_write(marker_fd, "helper(): stop helping on cv %p\n", &count_threshold_cv);
		ftrace_write(marker_fd, "Removing helper() thread: pid %d prio 93\n", my_pid);
	}
	pthread_exit(NULL);
}

void *waiter(void *d) 
{
	int ret;
	struct timespec twait;
	struct sched_param param;
	cpu_set_t mask;
	pid_t my_pid = gettid();
	
	sleep(1);

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
	ftrace_write(marker_fd, "Starting waiter(): pid %d prio 95\n", my_pid);
	twait = usec_to_timespec(500000L);
	busywait(&twait);
	
	/*
	Lock mutex and wait for signal.  Note that the pthread_cond_wait routine
	will automatically and atomically unlock mutex while it waits. 
	*/
	pthread_mutex_lock(&count_mutex);
	ftrace_write(marker_fd, "waiter(): pid %d. Going into wait...\n", my_pid);
	ftrace_write(marker_fd, "waiter(): waits on cv %p\n", &count_threshold_cv);
	pthread_cond_wait(&count_threshold_cv, &count_mutex);
	ftrace_write(marker_fd, "waiter(): wakes on cv %p\n", &count_threshold_cv);
	/* "Consume" the item... */
	ftrace_write(marker_fd, "waiter(): pid %d Condition signal received.\n", my_pid);
	ftrace_write(marker_fd, "waiter(): pid %d Consuming an item...\n", my_pid);
	twait = usec_to_timespec(2000000L);
	busywait(&twait);
	
	ftrace_write(marker_fd, "waiter(): pid %ld Unlocking mutex.\n", my_pid);
	pthread_mutex_unlock(&count_mutex);

	pthread_exit(NULL);
}

void *annoyer(void *d)
{
	int ret;
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
	
	param.sched_priority = 94;
	ret = pthread_setschedparam(pthread_self(), 
				    SCHED_FIFO, 
				    &param);
	if (ret != 0) {
		printf("pthread_setschedparam failed\n"); 
		exit(EXIT_FAILURE);
	}
	ftrace_write(marker_fd, "Starting annoyer(): pid %d prio 94\n", my_pid);

	ftrace_write(marker_fd, "annoyer(): should preempt inc_count for 5sec\n");

	twait = usec_to_timespec(5000000L);
	ftrace_write(marker_fd, "annoyer(): starts running...\n");
	busywait(&twait);

	ftrace_write(marker_fd, "annoyer(): dies...\n");
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
	pthread_mutexattr_init(&rt_mutex_attr);
	pthread_mutexattr_setprotocol(&rt_mutex_attr, PTHREAD_PRIO_INHERIT);
	pthread_mutex_init(&rt_mutex, &rt_mutex_attr);
	
	/* For portability, explicitly create threads in a joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	ftrace_write(marker_fd, "[main]: creating helper()\n");
	pthread_create(&threads[0], &attr, helper, NULL);
	ftrace_write(marker_fd, "[main]: creating rt_owner()\n");
	pthread_create(&threads[1], &attr, rt_owner, NULL);
	ftrace_write(marker_fd, "[main]: creating waiter()\n");
	pthread_create(&threads[2], &attr, waiter, NULL);
	sleep(3);
	ftrace_write(marker_fd, "[main]: creating annoyer()\n");
	pthread_create(&threads[3], &attr, annoyer, NULL);
	
	/* Wait for all threads to complete */
	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
	printf ("Main(): Waited and joined with %d threads. Done.\n", 
	        NUM_THREADS);
	
	if (trace_fd >= 0)
	        write(trace_fd, "0", 1);
	/* Clean up and exit */
	pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&count_mutex);
	pthread_cond_destroy(&count_threshold_cv);
	pthread_mutex_destroy(&rt_mutex);
	pthread_exit (NULL);
}
