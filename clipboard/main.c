#include <stdio.h>
#include <stdlib.h>

char *local_regions[];

int main(int argc, char *argv[])
{
	/* In order to launch the clipboard in connected mode it is necessary for
	 * the user to use the command  line  argument  -c followed  by  the
	 * address  and  port  of  another  clipboard.
	 * Although a clipboard can work in single or connected mode, it should be
	 * able to always receive connections from the local applications or remote
	 * clipboards.
	 */
	// TODO: to launch in connected mode, we need -c, but in single mode we also
	// need to accept connections?? that means we also need -c in single mode
	// so remote clipboards can reach us.

	if (argc > 1) {
		if (argc != 4) {
			puts("-c <address> <port>");
			exit(1);
		}
		// connected mode
		printf("%s listening on %s:%s\n", argv[0], argv[2], argv[3]);
	}
	else {
		// single mode
	}

	return EXIT_SUCCESS;
}
