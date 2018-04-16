#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/un.h>

#define mperror() { fprintf(stderr, "%s:%d errno = %d: %s\n", __FILE__, __LINE__-1, errno, strerror(errno)); exit(errno); }

// TODO: are apps single threaded? do we need to be thread safe?

#define CLIPBOARD_SOCKET "/c"

/*
 * This function is called by the application before interacting with the distributed clipboard.
 *
 * Arguments:
 * clipboard_dir – this argument corresponds to the directory where the local clipboard was launched (see section  3.1.1)
 *
 * The function return -1 if the local clipboard can not be accessed and a positive value in case of success. The returned value will be used in all other functions as clipboard_id.
*/
int clipboard_connect(char *clipboard_dir)
{
	int s, r;

	// Open a socket to the local clipboard server at `clipboard_dir`
	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s == -1) mperror();

	struct sockaddr_un clipboard_addr;
	clipboard_addr.sun_family = AF_UNIX;
	char path_buf[sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path)];
	strcpy(path_buf, clipboard_dir);
	strcat(path_buf, CLIPBOARD_SOCKET);
	strcpy(clipboard_addr.sun_path, path_buf);
	socklen_t clipboard_addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(clipboard_addr.sun_path) + 1;
	r = connect(s, (struct sockaddr *) &clipboard_addr, clipboard_addrlen);
	// Error
	if (r == -1) return r;

	// Return fd of the socket
	return s;
}

/*
 * This function copies the data pointed by buf to a region on the local clipboard.
 *
 * Arguments:
 * clipboard_id – this argument corresponds to the value returned by clipboard_connect
 * region – This argument corresponds to the identification of the region the user wants to copy the data to. This should be a value between 0 and 9.
 * buf – pointer to the data that is to be copied to the local clipboard
 * count – the length of the data pointed by buf.
 *
 * This function returns positive integer corresponding to the number of bytes copied or 0 in case of error (invalid clipboard_id, invalid region or local clipboard unavailable).
 */
int clipboard_copy(int clipboard_id, int region, void *buf, size_t count)
{
}

/*
 * This function copies from the system to the application the data in a certain region. The copied data is stored in the memory pointed by buf up to a length of count.
 *
 * Arguments:
 * clipboard_id – this argument corresponds to the value returned by clipboard_connect
 * region – This argument corresponds to the identification of the region the user wants to paste data from. This should be a value between 0 and 9.
 * buf – pointer to the data where the data is to be copied to
 * count – the length of memory region pointed by buf.
 *
 * This function returns a positive integer corresponding to the number of bytes copied or 0 in case of error (invalid clipboard_id, invalid region or local clipboard unavailable).
 */
int clipboard_paste(int clipboard_id, int region, void *buf, size_t count)
{
}

/*
 * This function waits for a change on a certain region ( new copy), and when it happens the new data in that region is copied to memory pointed by buf. The copied data is stored in the memory pointed by buf up to a length of count.
 *
 * Arguments:
 * clipboard_id – this argument corresponds to the value returned by clipboard_connect
 * region – This argument corresponds to the identification of the region the user wants to wait for. This should be a value between 0 and 9.
 * buf – pointer to the data where the data is to be copied to
 * count – the length of memory region pointed by buf.
 *
 * This function returns a positive integer corresponding to the number of bytes copied or 0 in case of error (invalid clipboard_id, invalid region or local clipboard unavailable).
*/
int clipboard_wait(int clipboard_id, int region, void *buf, size_t count)
{
}

/*
 * This function closes the connection between the application and the local clipboard.
 *
 * Arguments:
 * clipboard_id – this argument corresponds to the value returned by clipboard_connect
 */
void clipboard_close(int clipboard_id)
{
}
