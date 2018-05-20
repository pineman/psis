#pragma once

#include <signal.h>
#include <pthread.h>

int block_signals(sigset_t *sigset);
int init_mutex(pthread_mutex_t *mutex);
