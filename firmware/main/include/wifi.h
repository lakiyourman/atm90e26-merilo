#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>
#include "esp_err.h"

extern bool is_scanning;
extern char old_ssid[32];

void url_decode(char *dst, const char *src);

void wifi_init(void);

esp_err_t wifi_post_handler(httpd_req_t *req);

esp_err_t scan_get_handler(httpd_req_t *req);

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

#endif