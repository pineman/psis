#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define emperror(r) { fprintf(stderr, "%s:%d errno = %d: %s\n", __FILE__, __LINE__-1, r, strerror(r)); exit(r); }
#define mperror(r) { fprintf(stderr, "%s:%d errno = %d: %s\n", __FILE__, __LINE__-1, r, strerror(r)); }

#define CB_SOCKET "/CLIPBOARD_SOCKET"
#define CB_NUM_REGIONS 10
