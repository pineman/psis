#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/socket.h>

#include "cb_common.h"
#include "cb_msg.h"

// Check if count > 0, buf has content, and region is valid
bool cb_sanity_check(int region, size_t count, void *buf)
{
	// count must be >0 and buf must contain something
	if (count == 0 || buf == NULL) return false;

	// region must be 0..CB_NUM_REGIONS-1
	if (region >= CB_NUM_REGIONS || region < 0) return false;

	return true;
}

// Modify a buffer msg_buf to become a message with given arguments by value.
// Arguments are COPIED to msg_buf.
// Always add a \0 at the end => actual message size is data_size + 1
void make_msg(uint8_t *msg_buf, uint8_t cmd, uint8_t region, uint32_t data_size, void *data)
{
	int i = 0; // track msg_buf position

	// command (1 byte)
	msg_buf[i] = cmd;
	i += 1;

	// region (1 byte)
	msg_buf[i] = region;
	i += 1;

	// data_size (4 bytes)
	memcpy(msg_buf+i, (void *) &data_size, 4);
	i += 4;

	// data (`data_size` bytes)
	memcpy(msg_buf+i, data, data_size);
	i += data_size;
	// data_size is at most CB_DATA_MAX_SIZE-1, there's one char left saved
	msg_buf[i] = '\0'; // always guarantee at least one \0 when sending
}

// Send message to a clipboard server
// Returns number of user data bytes written (not including the header)
// or 0 on error
int send_msg(int clipboard_id, uint8_t cmd, uint8_t region, size_t count, void *buf)
{
	int r, ret;

	r = cb_sanity_check(region, count, buf);
	if (r == false) return 0;

	// Limit *data_size to CB_DATA_MAX_SIZE-1 bytes
	// (save one for extra \0)
	uint32_t data_size = (uint32_t) count;
	if (data_size > CB_DATA_MAX_SIZE-1) {
		data_size = CB_DATA_MAX_SIZE-1;
	}

	// data_size is at most CB_DATA_MAX_SIZE-1
	// => msg_size is at most CB_DATA_MAX_SIZE (+ 1 for extra \0)
	int msg_size = CB_HEADER_SIZE + data_size + 1;
	uint8_t *msg_buf = malloc(msg_size);
	if (msg_buf == NULL) emperror(errno);
	make_msg(msg_buf, cmd, region, data_size, buf);

	puts("send");
	for (int i = 0; i < msg_size; i++) {
		printf("%#x ", msg_buf[i]);
	}
	puts("");

	r = send(clipboard_id, msg_buf, msg_size, MSG_NOSIGNAL);
	if (r == -1) {
		if (errno == EBADF || errno == ECONNRESET || errno == EACCES || errno == EPIPE) {
			// Return zero when there's a connection error
			ret = 0;
			emperror(errno); // TODO: remove
		}
		else {
			emperror(errno);
		}
	}
	else if (r == msg_size) {
		// Success: send() returned the number of bytes we asked for
		// return number of user data bytes (all of them were written)
		ret = data_size;
	}
	else {
		// send() returned less bytes than we asked for
		// return number of user data bytes actually written, if any
		ret = r - CB_HEADER_SIZE;
		if (ret < 0) ret = 0;
	}

	free(msg_buf);

	return ret;
}

// Wrapper around recv()
// Returns number of bytes read from clipboard or 0 on error.
int erecv(int sock_fd, void *buf, size_t len)
{
	int r;

	r = recv(sock_fd, buf, len, MSG_WAITALL);
	if (r == -1) {
		if (errno == EBADF || errno == ECONNRESET || errno == EACCES || errno == EPIPE) {
			// Return zero when there's a connection error
			return 0;
			emperror(errno); // TODO: remove
		}
		else {
			emperror(errno);
		}
	}

	return r;
}

// Receive a message from the clipboard
// Returns number of clipboard data bytes read
int recv_msg(int clipboard_id, uint8_t *cmd, uint8_t *region, size_t count, void *buf)
{
	int r, ret = 0;

	// Allocate space to receive header
	uint8_t *header_buf = malloc(CB_HEADER_SIZE);
	if (header_buf == NULL) emperror(errno);

	// Get the message header
	r = erecv(clipboard_id, (void *) header_buf, CB_HEADER_SIZE);
	if (r == 0) goto out_hdr;
	puts("recv");
	for (int i = 0; i < CB_HEADER_SIZE; i++) {
		printf("%#x ", header_buf[i]);
	}

	// Get data_size from the header (1+1 bytes up)
	// + 1 for extra \0 originally sent
	uint32_t resp_data_size = 0;
	resp_data_size = ((uint32_t) *(header_buf+1+1)) + 1;

	// Allocate space to receive data
	uint8_t *data_buf = malloc(resp_data_size);
	if (data_buf == NULL) emperror(errno);

	// Get clipboard data
	ret = erecv(clipboard_id, (void *) data_buf, (size_t) resp_data_size);
	if (ret == 0) goto out_data;

	// command (1 byte)
	*cmd = header_buf[0];

	// region (1 byte)
	*region = header_buf[1];

	for (int i = 0; i < resp_data_size; i++) {
		printf("%#x ", data_buf[i]);
	}
	puts("");

	if (resp_data_size > count) {
		// User buf is too small for the message. Truncate and add a \0 at the end.
		memcpy(buf, (void *) data_buf, count);
		puts("recv: adding null");
		((char *) buf)[count-1] = '\0';
		ret = count;
	}
	else {
		// User buf big enough for the message, which should already contain
		// at least one \0 at the end, added by send_msg
		memcpy(buf, (void *) data_buf, resp_data_size);
		ret = resp_data_size - 1; // don't count the safety \0
	}

out_data:
	free(data_buf);
out_hdr:
	free(header_buf);

	// Return number of read clipboard data bytes
	return ret;
}