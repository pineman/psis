#include <stdio.h>
#include <string.h>
#include <errno.h>


#define mperror(err) do { char __buf__[1024]; sprintf(__buf__, "%s:%d %s(), errno = %d", __FILE__, __LINE__-1, __func__, err); perror(__buf__); } while(0);
#define emperror(err) do { mperror(err); exit(err); } while(0);

#include "libclipboard.h"


int main(int argc, char *argv[])
{
	int r = 0;
    long int region;
	char buf[100];


	if (argc < 2 || argc > 4) return EXIT_FAILURE;

    region = strtol(argv[2], NULL, 0);
    strncpy(buf, argv[3], sizeof(buf)-1);

	int cb = clipboard_connect(argv[1]);
	if (cb == -1) emperror(errno);

    r = clipboard_copy(cb, region, buf, strlen(buf) + 1);
    if (r == 0) { mperror(errno)}
    else {
        printf("Copied \"%s\" to clipboard region %ld\n", buf, region);
    }

	clipboard_close(cb);

	return EXIT_SUCCESS;
}
