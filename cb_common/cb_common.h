#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define mperror(err) do { char __buf__[1024]; sprintf(__buf__, "%s:%d errno = %d", __FILE__, __LINE__-1, err); perror(__buf__); } while(0);
#define emperror(err) do { mperror(err); exit(err); } while(0);

#define CB_SOCKET "CLIPBOARD_SOCKET"
#define CB_NUM_REGIONS 10
