#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stddef.h>

#define mperror(err) do { char __buf__[1024]; sprintf(__buf__, "%s:%d %s(), errno = %d", __FILE__, __LINE__-1, __func__, err); perror(__buf__); } while(0);
#define emperror(err) do { mperror(err); exit(err); } while(0);

#include "libclipboard.h"

int main(int argc, char *argv[])
{
	int r = 0;

	if (argc != 2) return EXIT_FAILURE;

	int cb = clipboard_connect(argv[1]);
	if (cb == -1) emperror(errno);

	char buf1[] = "Heilo";
	r = clipboard_copy(cb, 8, buf1, sizeof(buf1));
	if (r == 0) emperror(errno);
	printf("copy returned %d, sizeof(buf1) = %lu\n", r, sizeof(buf1));

	char buf2[3];
	r = clipboard_paste(cb, 8, (void *) buf2, sizeof(buf2));
	if (r == 0) emperror(errno);
	printf("paste returned %d: %s, sizeof(buf2) = %lu\n", r, buf2, sizeof(buf2));
	for (int i = 0; i < 3; i++ ) {
		printf("%d: %c\n", i, buf2[i]);
	}
	puts("");

	return EXIT_SUCCESS;
}
