#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../common.h"
#include "msg.h"

// TODO: make critical sections (i.e. read/write [connect?]) thread-safe

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
	int clipboard_fd; // fd of the socket to local clipboard server
	int r; // Auxiliary variable just for return values

	// Open a socket to the local clipboard server at local path `clipboard_dir`/CLIPBOARD_SOCKET
	clipboard_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (clipboard_fd == -1) eperror(errno);

	struct sockaddr_un clipboard_addr;
	clipboard_addr.sun_family = AF_UNIX;
	// Maximum size of sockaddr_un.sun_path for temp buffer
	int path_buf_size = sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path);
	char path_buf[path_buf_size];
	// Copy `clipboard_dir` and concatenate with `CLIPBOARD_SOCKET` to get the
	// socket path.
	strcpy(path_buf, clipboard_dir);
	strcat(path_buf, CLIPBOARD_SOCKET);
	strcpy(clipboard_addr.sun_path, path_buf);
	socklen_t clipboard_addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(clipboard_addr.sun_path) + 1;

	// Connect to local clipboard server
	r = connect(clipboard_fd, (struct sockaddr *) &clipboard_addr, clipboard_addrlen);
	// Error connecting, errno is set
	if (r == -1) return -1;

	// Return the fd of the socket to local clipboard server
	return clipboard_fd;
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
	// count must be >0 and buf must contain something
	if (count == 0 || buf == NULL) return 0;

	// region must be 0..NUM_REGIONS-1
	if (region >= NUM_REGIONS || region < 0) return 0;

	// Truncate data to MSG_DATA_MAX_SIZE bytes
	uint32_t data_size = (uint32_t) count;
	if (data_size > MSG_DATA_MAX_SIZE) {
		data_size = MSG_DATA_MAX_SIZE;
	}

	uint8_t *msg_copy = make_msg(CB_CMD_COPY, (uint8_t) region, data_size, buf);

	int ret = send_msg(clipboard_id, data_size, msg_copy);

	free(msg_copy);
	return ret;
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
	int ret;

	uint8_t *msg_req_paste = make_msg(CB_CMD_REQ_PASTE, (uint8_t) region, 0, NULL);

	ret = send_msg(clipboard_id, 0, msg_req_paste);
	if (ret == 0) goto out;

out:
	free(msg_req_paste);
	return ret;
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
	int r;

	r = close(clipboard_id);
	if (r == -1) eperror(errno);
}
