#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#define mperror(err) do { char __buf__[1024]; sprintf(__buf__, "%s:%d %s(), errno = %d", __FILE__, __LINE__-1, __func__, err); perror(__buf__); } while(0);
#define emperror(err) do { mperror(err); exit(err); } while(0);

#include "libclipboard.h"

int main(int argc, char *argv[])
{
	int r = 0;

	if (argc != 2) return EXIT_FAILURE;

	int cb = clipboard_connect(argv[1]);
	if (cb == -1) emperror(errno);

	char buf[40];
	int j = 0;
	while (1) {
		for (int i = 0; i < 10; i++ ) {
			r = clipboard_paste(cb, i, buf, 4);
			if (r == 0) { mperror(errno); break; }
			printf("%d \"%s\" %d\n", i, buf, j++);
			//sleep(1);
		}
		if (r == 0) break;
	}

	clipboard_close(cb);

	return EXIT_SUCCESS;
}
