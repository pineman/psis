uint8_t *make_msg(uint8_t cmd, uint8_t region, uint32_t data_size, void *data);
int send_msg(int clipboard_id, uint32_t data_size, uint8_t *msg);
uint8_t *recv_msg(int clipboard_id, int *msg_size);
