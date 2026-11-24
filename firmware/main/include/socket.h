#ifndef SOCKET_H
#define SOCKET_H

#define MAX_WS_CLIENTS  4


extern int s_client_fds[MAX_WS_CLIENTS];

void ws_client_add(int fd);

void ws_client_remove(int fd);

esp_err_t ws_handler(httpd_req_t *req);

void websocket_broadcast_json(const char *json);


#endif