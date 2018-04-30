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

int main(int argc, char *argv[])
{
	int r;

	if (argc != 2)
		return EXIT_FAILURE;

	int cb = clipboard_connect(argv[1]);

	char buf[] = "";
	r = clipboard_copy(cb, 1, NULL, 2);
	printf("r = %d\n", r);

	return EXIT_SUCCESS;
}
