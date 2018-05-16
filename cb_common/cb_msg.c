#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <sys/socket.h>

#include "cb_common.h"
#include "cb_msg.h"

// TODO: deletes wrappers?
int esend(int sock_fd, void *buf, size_t len)
{
	// MSG_NOSIGNAL: make send() not generate SIGPIPE // TODO: maybe not
	//return send(sock_fd, buf, len, MSG_NOSIGNAL);
	return send(sock_fd, buf, len, 0);
}
int erecv(int sock_fd, void *buf, size_t len)
{
	// MSG_WAITALL: block untill recv() can return `len` bytes
	return recv(sock_fd, buf, len, MSG_WAITALL);
}

// TODO: send_msg and recv_msg can receive a mutex argument protecting
// `clipboard_id`

// Send message to a clipboard server
int send_msg(int clipboard_id, uint8_t cmd, uint8_t region, uint32_t data_size)
{
	int r;

	// cmd must be valid
	assert(cmd < CB_NUM_CMDS);
	// region must be 0..CB_NUM_REGIONS-1
	assert(region > 0);
	assert(region <= CB_NUM_REGIONS-1);

	uint8_t msg[CB_MSG_SIZE];
	// command (1 byte)
	msg[0] = cmd;
	// region (1 byte)
	msg[1] = region;
	// data_size (4 bytes)
	memcpy(msg+2, (void *) &data_size, 4);

	// Send message
	r = esend(clipboard_id, (void *) msg, CB_MSG_SIZE);
	// return 0 on error or if couldn't send the message
	if (r == -1 || r == 0 || r != CB_MSG_SIZE) return 0;

	return r;
}

// Receive a message from the clipboard
int recv_msg(int clipboard_id, uint8_t *cmd, uint8_t *region, uint32_t *data_size)
{
	int r;

	assert(cmd != NULL);
	assert(region != NULL);

	uint8_t msg[CB_MSG_SIZE];
	// Get the message
	r = erecv(clipboard_id, (void *) msg, CB_MSG_SIZE);
	// return 0 on error or if couldn't get the message
	if (r == -1 || r == 0 || r != CB_MSG_SIZE) return 0;

	// cmd must be valid
	assert(msg[0] < CB_NUM_CMDS);
	// region must be 0..CB_NUM_REGIONS-1
	assert(msg[1] > 0);
	assert(msg[1] <= CB_NUM_REGIONS-1);

	// command (1 byte)
	*cmd = msg[0];
	// region (1 byte)
	*region = msg[1];
	// data_size (4 bytes)
	*data_size = ((uint32_t) *(msg+1+1));

	return r;
}
