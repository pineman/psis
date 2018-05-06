#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Message is: <command><region><size><data> == <header><data>
 * `command` is 1 byte `uint8_t`
 * `region` is 1 byte `uint8_t`
 * `data_size` is 4 bytes `uint32_t`
 * `data` is whatever `size`
 */

enum cb_cmd {CB_CMD_COPY, CB_CMD_REQ_PASTE, CB_CMD_PASTE};
#define CB_REQ_PASTE_DATA "req_paste"
#define CB_REQ_PASTE_DATA_SIZE 10
#define CB_HEADER_SIZE 1 + 1 + 4
#define CB_DATA_MAX_SIZE 10+1

void make_msg(uint8_t *msg_buf, uint8_t cmd, uint8_t region, uint32_t data_size, void *data);
int send_msg(int clipboard_id, uint8_t cmd, uint8_t region, size_t count, void *buf);

int erecv(int sock_fd, void *buf, size_t len);
int recv_msg(int clipboard_id, uint8_t *cmd, uint8_t *region, size_t count, void *buf);
