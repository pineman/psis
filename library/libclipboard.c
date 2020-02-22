#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "cb_common.h"
#include "cb_msg.h"

#include "libclipboard.h"

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
	if (clipboard_fd == -1) return -1;

	struct sockaddr_un clipboard_addr;
	clipboard_addr.sun_family = AF_UNIX;
	// Maximum size of sockaddr_un.sun_path for temp buffer
	char path_buf[sizeof(clipboard_addr.sun_path)];
	// Copy `clipboard_dir` and concatenate with `CLIPBOARD_SOCKET` to get the
	// socket path.
	strcpy(path_buf, clipboard_dir);
	strcat(path_buf, "/");
	strcat(path_buf, CB_SOCKET);
	strcpy(clipboard_addr.sun_path, path_buf);
	socklen_t clipboard_addrlen = sizeof(clipboard_addr);

	// Try to set SO_NOSIGPIPE.
	r = cb_setsockopt(clipboard_fd);
	if (r == -1) return -1;

	// Connect to local clipboard server
	r = connect(clipboard_fd, (struct sockaddr *) &clipboard_addr, clipboard_addrlen);
	// Error connecting, errno is set
	if (r == -1) return -1;

	// Return the fd of the socket to local clipboard server
	return clipboard_fd;
}

// Check for potencially fatal errors in the arguments coming the app.
static void sanity_check(void *buf, size_t count)
{
	// buf must be a valid pointer
	assert(buf != NULL);
	// count must not be 0, and must be less than UINT32_MAX
	assert(count != 0);
	assert(count <= UINT32_MAX);
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
	ssize_t r;

	// Check for fatal errors in arguments
	sanity_check(buf, count);

	// Send the 'copy' message
	r = cb_send_msg(clipboard_id, CB_CMD_COPY, (uint8_t) region, (uint32_t) count);
	// return 0 on error, errno is set
	// error might be all the normal errors plus EAGAIN/EWOULDBLOCK because
	// clipboard_connect set a send timeout. It's up to the app to investigate
	// errno.
	if (r == -1) return 0;
	// Fatal error: app tried to send invalid message.
	assert(r != -2);

	// Get the server's response
	uint8_t resp_cmd;
	uint8_t resp_region;
	uint32_t resp_data_size;
	r = cb_recv_msg(clipboard_id, &resp_cmd, &resp_region, &resp_data_size);
	if (r == -1 || r == 0) return 0; // Receiving failed
	// Fatal error: server sent invalid message.
	assert(r != -2);
	// Fatal error: server did not reply correctly
	assert(resp_region == region);
	if (resp_cmd == CB_CMD_RESP_COPY_NOK) return 0;
	assert(resp_cmd == CB_CMD_RESP_COPY_OK);

	// Send data
	r = cb_send(clipboard_id, (void *) buf, count);
	if (r == -1) return 0;
	return (int) r;
}

static int do_paste(uint8_t cmd, int clipboard_id, int region, void *buf, size_t count)
{
	ssize_t r;

	// Check for fatal errors in arguments
	sanity_check(buf, count);

	// Send a paste request (data_size = 0, no data is sent)
	r = cb_send_msg(clipboard_id, cmd, (uint8_t) region, 0);
	// return 0 on error, errno is set
	if (r == -1) return 0;
	// Fatal error: app tried to send invalid message.
	assert(r != -2);

	// Get the server's response
	uint8_t resp_cmd;
	uint8_t resp_region;
	uint32_t resp_data_size;
	r = cb_recv_msg(clipboard_id, &resp_cmd, &resp_region, &resp_data_size);
	if (r == -1 || r == 0) return 0; // Receiving failed
	// Fatal error: server sent invalid message.
	assert(r != -2);

	// Fatal error: server did not reply correctly
	assert(resp_region == region);
	assert(resp_cmd == CB_CMD_PASTE);

	char *resp_data = malloc(resp_data_size);
	if (resp_data == NULL) return 0;
	// Get clipboard data
	r = cb_recv(clipboard_id, (void *) resp_data, resp_data_size);
	if (r == -1 || r == 0) { free(resp_data); return 0; } // Receiving failed

	assert(resp_data_size > 0);
	size_t copy_size = resp_data_size;
	if (resp_data_size > count)
		copy_size = count;
	memcpy(buf, resp_data, copy_size);
	if (((char *) buf)[copy_size-1] != '\0') {
		// data from server was not null terminated (or buf was not big enough
		// for it and it was truncated). Add a '\0'.
		((char *)buf)[copy_size-1] = '\0'; // Possibly lost a byte of data
	}
	free(resp_data);

	// Return number of bytes the server sent. Might be less than `count`! This allows
	// the app to know it didn't have a buffer big enough for the whole region.
	return (int) resp_data_size;
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
	return do_paste(CB_CMD_REQ_PASTE, clipboard_id, region, buf, count);
}

/*
 * This function waits for a change on a certain region (new copy), and when it happens the new data in that region is copied to memory pointed by buf. The copied data is stored in the memory pointed by buf up to a length of count.
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
	return do_paste(CB_CMD_REQ_WAIT, clipboard_id, region, buf, count);
}

/*
 * This function closes the connection between the application and the local clipboard.
 *
 * Arguments:
 * clipboard_id – this argument corresponds to the value returned by clipboard_connect
 */
void clipboard_close(int clipboard_id)
{
	close(clipboard_id);
}
