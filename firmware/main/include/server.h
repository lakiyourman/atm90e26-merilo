#ifndef SERVER_H
#define SERVER_H

typedef enum {
    IDLE,
    CONNECTING,
    CONNECTED,
    FAILED
} conn_status_t;

extern conn_status_t current_status;
extern httpd_handle_t s_server;

esp_err_t status_get_handler(httpd_req_t *req);

esp_err_t info_get_handler(httpd_req_t *req);

esp_err_t root_get_handler(httpd_req_t *req);

httpd_handle_t start_web_server(void);

#endif