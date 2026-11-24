#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

#include "socket.h"
#include "server.h"

static const char *TAG = "socket";

int s_client_fds[MAX_WS_CLIENTS];

void ws_client_add(int fd)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_client_fds[i] < 0) {
            s_client_fds[i] = fd;
            ESP_LOGI(TAG, "WS client connected fd=%d", fd);
            return;
        }
    }
    ESP_LOGW(TAG, "WS client list full, ignoring fd=%d", fd);
}

void ws_client_remove(int fd)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_client_fds[i] == fd) {
            s_client_fds[i] = -1;
            ESP_LOGI(TAG, "WS client disconnected fd=%d", fd);
            return;
        }
    }
}

//WebSocket handler
esp_err_t ws_handler(httpd_req_t *req) {
    // If the browser sends a message, it lands here.
    // We don't need the "if (req->method == HTTP_GET)" block here anymore
    // because that's handled by my_ws_post_handshake_cb now.

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Frame recv failed");
        return ret;
    }
    
    // Optional: Log if client sends something
    if (ws_pkt.len > 0) {
        ESP_LOGI(TAG, "Received %d bytes from client", ws_pkt.len);
    }

    return ESP_OK;
}

void websocket_broadcast_json(const char *json)
{
    if (!json || !s_server) return;
    
    httpd_ws_frame_t ws_pkt = {
        .payload  = (uint8_t *)json,
        .len      = strlen(json),
        .type     = HTTPD_WS_TYPE_TEXT,
        .final    = true,
    };

    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_client_fds[i] < 0) continue;

        esp_err_t ret = httpd_ws_send_frame_async(s_server, s_client_fds[i], &ws_pkt);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Slanje klijentu fd=%d nije uspelo (%s), uklanjam",
                     s_client_fds[i], esp_err_to_name(ret));
            ESP_LOGE(TAG, "Failed to send: %s", esp_err_to_name(ret));
            s_client_fds[i] = -1;
        }
    }
}   