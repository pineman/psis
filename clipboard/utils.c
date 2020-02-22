
#include <unistd.h>
#include <signal.h>
#include <pthread.h>


int block_signals(sigset_t *sigset)
{
	int r;

	r = sigfillset(sigset);
	if (r == -1) return -1;

	// Don't ignore stop/continue signals
	r = sigdelset(sigset, SIGSTOP);
	if (r == -1) return -1;
	r = sigdelset(sigset, SIGTSTP);
	if (r == -1) return -1;
	r = sigdelset(sigset, SIGCONT);
	if (r == -1) return -1;

	// Set thread sigmask
	r = pthread_sigmask(SIG_SETMASK, sigset, NULL);
	if (r == -1) return -1;

	return 0;
}

int init_mutex(pthread_mutex_t *mutex)
{
	int r;
	pthread_mutexattr_t mutex_attr;
	r = pthread_mutexattr_init(&mutex_attr);
	if (r != 0) return r;

	r = pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
	if (r != 0) return r;

	r = pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);
	if (r != 0) return r;

	r = pthread_mutex_init(mutex, &mutex_attr);
	if (r != 0) return r;

	r = pthread_mutexattr_destroy(&mutex_attr);
	if (r != 0) return r;

	return 0;
}
