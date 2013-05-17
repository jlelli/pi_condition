/******************************************************************************
* FILE: pi_cond_helper_test.c
* DESCRIPTION:
*   Example code for demonstrating how to add or delete helpers for a condition
*   variable.
*
* LAST REVISED: 05/17/13  Juri Lelli
******************************************************************************/
#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "rt-app_utils.h"
#include "libcv/dl_syscalls.h"

int count = 0;
pthread_cond_t count_threshold_cv;

void *helper(void *t) 
{
	int ret;
	long my_id = (long)t;
	pid_t my_pid = gettid();

	printf("Starting helper(): thread %ld\n", my_id);
	
	/* Adds itself to the helpers list. */
	printf("helper(): thread %ld is going to be an helper...\n", my_id);
	pthread_cond_helpers_add(&count_threshold_cv, my_pid);
	printf("helper() [thread %ld]: I'm an helper!\n", my_id);

	sleep(2);

	/* Removes itself from the helpers list. */
	printf("helper(): thread %ld is going to not be an helper anymore ...\n",
	       my_id);
	pthread_cond_helpers_del(&count_threshold_cv, my_pid);
	printf("helper() [thread %ld]: I'm not an helper anymore!\n", my_id);


	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int i, rc, ret; 
	long t1=1, t2=2, t3=3;
	pthread_t thread;
	pthread_attr_t attr;
	
	/* Initialize condition variable object */
	pthread_cond_init (&count_threshold_cv, NULL);
	
	/* For portability, explicitly create threads in a joinable state */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&thread, &attr, helper, (void *)t1);
	
	pthread_join(thread, NULL);
	printf ("Main(): Waited and joined with helper thread. Done.\n");
	
	/* Clean up and exit */
	pthread_attr_destroy(&attr);
	pthread_cond_destroy(&count_threshold_cv);
	pthread_exit (NULL);
}
