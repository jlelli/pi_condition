#include "dl_syscalls.h"

int sched_setscheduler2(pid_t pid, int policy,
			  const struct sched_param2 *param)
{
	return syscall(__NR_sched_setscheduler2, pid, policy, param);
}

int sched_setparam2(pid_t pid,
		      const struct sched_param2 *param)
{
	return syscall(__NR_sched_setparam2, pid, param);
}

int sched_getparam2(pid_t pid, struct sched_param2 *param)
{
	return syscall(__NR_sched_getparam2, pid, param);
}

int pthread_cond_helpers_add(pthread_cond_t *cond, pid_t pid)
{
	unsigned int *pcond;

	pcond = &cond->__data.__futex;

	return syscall(__NR_futex, pcond, FUTEX_COND_HELPER_MAN_PRIVATE,
		       pid, NULL, NULL, 1);
}

int pthread_cond_helpers_del(pthread_cond_t *cond, pid_t pid)
{
	unsigned int *pcond;

	pcond = &cond->__data.__futex;

	return syscall(__NR_futex, pcond, FUTEX_COND_HELPER_MAN_PRIVATE,
		       pid, NULL, NULL, 0);
}
