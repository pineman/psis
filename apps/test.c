#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stddef.h>

#define eperror(r) { fprintf(stderr, "%s:%d errno = %d: %s\n", __FILE__, __LINE__-1, r, strerror(r)); exit(r); }

#include "libclipboard.h"
#include "common.h"

int main(int argc, char *argv[])
{
	int r;

	if (argc != 2) return EXIT_FAILURE;

	int cb = clipboard_connect(argv[1]);
	printf("cb = %d\n", cb);
	if (cb == -1) return EXIT_FAILURE;

	char buf1[] = "Hello from app!";
	r = clipboard_copy(cb, 8, buf1, sizeof(buf1));
	printf("copy returned %d\n", r);
	if (r == 0) return EXIT_FAILURE;

	char buf2[10] = {0};
	r = clipboard_paste(cb, 8, (void *) buf2, sizeof(buf2));
	printf("paste returned %d: %s\n", r, buf2);
	if (r == 0) return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
