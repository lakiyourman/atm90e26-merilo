#include "esp_mac.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_log.h"

#include "server.h"
#include "wifi.h"
#include "socket.h"

static const char *TAG = "server";

httpd_handle_t s_server = NULL;
conn_status_t current_status = IDLE;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

esp_err_t my_ws_post_handshake_cb(httpd_req_t *req) {
    int fd = httpd_req_to_sockfd(req);
    ESP_LOGI("server", "Post-handshake callback for fd: %d", fd);
    
    // THIS is where we now add the client to your list
    ws_client_add(fd);
    
    return ESP_OK;
}

esp_err_t status_get_handler(httpd_req_t *req) {
    const char* status_text = "Unknown";
    if (current_status == IDLE) status_text = "Idle";
    else if (current_status == CONNECTING) status_text = "Connecting...";
    else if (current_status == CONNECTED) status_text = "Success! Connected.";
    else if (current_status == FAILED) status_text = "Failed: Check Password.";

    // --- DODATO ZA CORS ---
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    httpd_resp_send(req, status_text, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t info_get_handler(httpd_req_t *req) {
    char *json_str = malloc(512); 
    if (json_str == NULL) return ESP_ERR_NO_MEM;

    uint8_t mac_raw[6];
    char mac_pretty[18];
    esp_read_mac(mac_raw, ESP_MAC_WIFI_STA);
    snprintf(mac_pretty, sizeof(mac_pretty), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_raw[0], mac_raw[1], mac_raw[2], mac_raw[3], mac_raw[4], mac_raw[5]);

    wifi_config_t current_conf;
    esp_wifi_get_config(WIFI_IF_AP, &current_conf);

    snprintf(json_str, 512, 
         "{\"ssid\":\"%s\",\"mac\":\"%s\",\"ap_ssid\":\"%s\",\"ap_pass\":\"%s\"}",
         old_ssid, mac_pretty, current_conf.ap.ssid, current_conf.ap.password);
         
    // --- DODATO ZA CORS ---
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

    free(json_str);
    return ESP_OK;
}

// HTTP GET handler for root "/"
esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *html_page = (const char *)index_html_start;
    size_t html_len = index_html_end - index_html_start;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, html_len);
    return ESP_OK;
}

// Start the HTTP server
httpd_handle_t start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    // Jako bitno: Povećaj broj socket-a da podržiš i web i WS klijente
    config.max_open_sockets = MAX_WS_CLIENTS + 3; 
    
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // PRVO: Dodeli handle globalnoj varijabli!
        s_server = server; 

        ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
        
        // 1. Root handler
        httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(server, &uri_get);

        // 2. WiFi Scan
        httpd_uri_t uri_scan = { .uri = "/scan", .method = HTTP_GET, .handler = scan_get_handler };
        httpd_register_uri_handler(server, &uri_scan);

        // 3. WiFi Set
        httpd_uri_t uri_wifi = { .uri = "/set_wifi", .method = HTTP_POST, .handler = wifi_post_handler };
        httpd_register_uri_handler(server, &uri_wifi);

        // 4. Info & Status
        httpd_uri_t uri_info = { .uri = "/info", .method = HTTP_GET, .handler = info_get_handler };
        httpd_register_uri_handler(server, &uri_info);
        
        httpd_uri_t status_uri = { .uri = "/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(server, &status_uri);

        // 5. WebSocket (Sada koristiš 'server' ili 's_server', oba su validna)
        httpd_uri_t ws_uri = {
            .uri          = "/ws",
            .method       = HTTP_GET,
            .handler      = ws_handler,
            .is_websocket = true,
            .handle_ws_control_frames = true,
            .ws_post_handshake_cb = my_ws_post_handshake_cb
        };
        httpd_register_uri_handler(server, &ws_uri);  

        return server;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return NULL;
}