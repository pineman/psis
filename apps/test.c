#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stddef.h>

#include "libclipboard.h"
#include "cb_common.h"

int main(int argc, char *argv[])
{
	int r;

	if (argc != 2) return EXIT_FAILURE;

	int cb = clipboard_connect(argv[1]);
	if (cb == -1) return EXIT_FAILURE;

	char buf1[] = "Heilo";
	r = clipboard_copy(cb, 8, buf1, sizeof(buf1));
	printf("copy returned %d, sizeof(buf1) = %lu\n", r, sizeof(buf1));
	if (r == 0) return EXIT_FAILURE;

	char buf2[3] = {0};
	r = clipboard_paste(cb, 8, (void *) buf2, sizeof(buf2));
	printf("paste returned %d: %s, sizeof(buf2) = %lu\n", r, buf2, sizeof(buf2));
	if (r == 0) return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
