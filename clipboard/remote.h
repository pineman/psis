#pragma once

int listen_remote(void);
void *remote_accept_loop(void *arg);
void *serve_remote_client(void *arg);
