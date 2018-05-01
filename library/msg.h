#include <stddef.h>
#include <stdint.h>

void make_msg(uint8_t *msg_buf, uint8_t cmd, uint8_t region, uint32_t data_size, void *data);
int send_msg(int clipboard_id, uint32_t data_size, uint8_t *msg);

void parse_msg(uint8_t *msg_buf, uint8_t *cmd, uint8_t *region, uint32_t *data_size, void **data);
int erecv(int sock_fd, void *buf, size_t len);
int recv_msg(int clipboard_id, uint8_t *msg_buf);
