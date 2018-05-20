#pragma once

int listen_child(void);

void *child_accept(void *arg);
void cleanup_child_accept(void *arg);

void *serve_child(void *arg);
void cleanup_serve_child(void *arg);
