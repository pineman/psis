#pragma once

int listen_child(void);
void *child_accept(void *arg);
void *serve_child(void *arg);
