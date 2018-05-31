#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <unistd.h>
#include <time.h>

#define mperror(err) do { char __buf__[1024]; sprintf(__buf__, "%s:%d %s(), errno = %d", __FILE__, __LINE__-1, __func__, err); perror(__buf__); } while(0);
#define emperror(err) do { mperror(err); exit(err); } while(0);

#include "libclipboard.h"

#include <unistd.h>

int main(int argc, char *argv[])
{
	int r = 0;
    long int repetitions;

	if (argc < 2 || argc > 3) return EXIT_FAILURE;

    if (argc == 2) { repetitions = 1; }
    else {
        repetitions = strtol(argv[2], NULL, 0);
    }
    printf("repetitions: %ld\n", repetitions);

	int cb = clipboard_connect(argv[1]);
	if (cb == -1) emperror(errno);

	char buf[100];

	while (repetitions != 0) {
		for (int i = 0; i < 10; i++ ) {
			r = clipboard_paste(cb, i, buf, sizeof(buf));
			if (r == 0) { mperror(errno); break; }
			printf("%d \"%s\"\n", i, buf);
		}
        printf("=============\n");
        sleep(1);
		if (r == 0) break;

        if(repetitions != -1) repetitions--;
	}

	clipboard_close(cb);

	return EXIT_SUCCESS;
}

void print_clipboard(void){

}
