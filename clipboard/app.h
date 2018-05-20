#pragma once

int listen_local(void);
void *app_accept(void *arg);
void cleanup_app_accept(void *arg);
void *serve_app(void *arg);
void cleanup_serve_app(void *arg);
