#pragma once

int listen_local(void);
void *app_accept(void *arg);
void *serve_app(void *arg);
