#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stddef.h>

#define mperror() { fprintf(stderr, "%s:%d errno = %d: %s\n", __FILE__, __LINE__-1, errno, strerror(errno)); exit(errno); }

#include "../library/libclipboard.h"

int main(int argc, char *argv[])
{
	printf("%d\n", clipboard_connect("/home/pineman/ist/a3s2/PSis/proj/clipboard"));

	return EXIT_SUCCESS;
}
