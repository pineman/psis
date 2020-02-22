#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <time.h>

#define mperror(err) do { char __buf__[1024]; sprintf(__buf__, "%s:%d %s(), errno = %d", __FILE__, __LINE__-1, __func__, err); perror(__buf__); } while(0);
#define emperror(err) do { mperror(err); exit(err); } while(0);

#include "libclipboard.h"


int main(int argc, char *argv[])
{
	int r = 0;

	if (argc != 2) return EXIT_FAILURE;

	int cb = clipboard_connect(argv[1]);
	if (cb == -1) emperror(errno);

	char buf[100];
	int j = 0;
	struct timespec t = { .tv_sec = 1, .tv_nsec = 0 };
	while (1) {
		for (int i = 0; i < 10; i++ ) {
			sprintf(buf, "%d Hello from app", j++);
			r = clipboard_copy(cb, i, buf, strlen(buf) + 1);
			if (r == 0) { mperror(errno); break; }
			//printf("%d \"%s\" \n", i, buf);
			nanosleep(&t, NULL);
		}
		if (r == 0) break;
	}

	clipboard_close(cb);

	return EXIT_SUCCESS;
}
