#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "ctype.h"
#include "esp_mac.h"

#include "lcd.h"
#include "wifi.h"
#include "server.h"

#define MAXIMUM_RETRY 5

static char webapp_ip[8] = {0};
static char webapp_ip2[8] = {0};

static const char *TAG = "wifi";

bool is_scanning = false;
char old_ssid[32] = {0};
static char old_pass[64] = {0};
static int s_retry_num = 0;
static int wifi_rst = 0;

// Forward declaration matching the global signature in wifi.h
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default Wi-Fi STA/AP interfaces
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    // Initialize Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL, NULL));

    // Configure Access Point (AP)
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "Osiguraci_wifi_esp32",
            .ssid_len = strlen("Osiguraci_wifi_esp32"),
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Disable power-save mode for better web server responsiveness
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG, "AP Started. SSID: %s", "Osiguraci_wifi_esp32");
}

void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit((unsigned char)a) && isxdigit((unsigned char)b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

esp_err_t wifi_post_handler(httpd_req_t *req) {
    char content[128];
    int ret = httpd_req_recv(req, content, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    content[ret] = '\0';

    char ssid[32] = {0};
    char pwd[64] = {0};
    char decoded_ssid[32] = {0};
    char decoded_pwd[64] = {0};

    // Add CORS header so modern browsers accept the response safely
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (httpd_query_key_value(content, "ssid", ssid, sizeof(ssid)) == ESP_OK &&
        httpd_query_key_value(content, "password", pwd, sizeof(pwd)) == ESP_OK) {

        url_decode(decoded_ssid, ssid);
        url_decode(decoded_pwd, pwd);

        ESP_LOGI(TAG, "Attempting connection to SSID: %s", decoded_ssid);
        
        httpd_resp_send(req, "Connecting to new network... AP may close.", HTTPD_RESP_USE_STRLEN);
        current_status = CONNECTING;

        // 1. Force state reset to fix "STA is connecting, cannot set config" timing collisions
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));

        // 2. Safely activate APSTA mode
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if (mode == WIFI_MODE_AP) {  
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        }

        // 3. Populate pristine configuration matrix
        wifi_config_t wifi_config = {0}; 
        strlcpy((char *)wifi_config.sta.ssid, decoded_ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, decoded_pwd, sizeof(wifi_config.sta.password));
        
        // Dynamic threshold enables connection negotiation with WPA2/WPA3 routers seamlessly
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        wifi_config.sta.pmf_cfg.capable = true;
        wifi_config.sta.pmf_cfg.required = false;

        // 4. Commit and execute connection run
        s_retry_num = 0; // Reset counter for the fresh network target
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());

    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID or Password");
    }

    return ESP_OK;
}

esp_err_t scan_get_handler(httpd_req_t *req) {
    is_scanning = true;
    
    wifi_mode_t original_mode;
    esp_wifi_get_mode(&original_mode);
    
    if (original_mode == WIFI_MODE_AP) {  
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    }
    
    wifi_scan_config_t scan_config = {0}; 
    // Blocking scan sequence execution
    esp_wifi_scan_start(&scan_config, true);

    // 1. Fetch scanned targets
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 20) ap_count = 20;
    
    wifi_ap_record_t ap_info[20];
    esp_wifi_scan_get_ap_records(&ap_count, ap_info);

    // 2. Fetch runtime connected target
    wifi_ap_record_t active_ap;
    char active_ssid[33] = {0};
    bool is_currently_connected = false;

    if (esp_wifi_sta_get_ap_info(&active_ap) == ESP_OK) {
        strlcpy(active_ssid, (char*)active_ap.ssid, sizeof(active_ssid));
        is_currently_connected = true;
    }

    // 3. String allocation block
    char *json_str = malloc(2048); 
    if (json_str == NULL) return ESP_ERR_NO_MEM;

    strcpy(json_str, "[");
    for (int i = 0; i < ap_count; i++) {
        char item[180];
        const char* current_ssid = (char*)ap_info[i].ssid;
        
        bool is_active = (is_currently_connected && strcmp(current_ssid, active_ssid) == 0);
        bool is_open = (ap_info[i].authmode == WIFI_AUTH_OPEN);

        snprintf(item, sizeof(item), 
                 "{\"ssid\":\"%s\",\"rssi\":%d,\"connected\":%s,\"open\":%s}%s",
                 current_ssid, 
                 ap_info[i].rssi, 
                 is_active ? "true" : "false", 
                 is_open ? "true" : "false",
                 (i == ap_count - 1) ? "" : ",");
        
        strcat(json_str, item);
    }
    strcat(json_str, "]");

    // CORS configuration injection
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // 4. Send Response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    
    free(json_str);
    is_scanning = false;

    // Restore radio modes cleanly
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);
    if (current_mode != original_mode) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(original_mode));
    }
    
    return ESP_OK;
}

// Global scope definition matching your .h declaration
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    wifi_config_t current_conf;
    esp_wifi_get_config(WIFI_IF_STA, &current_conf);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {   
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "A device joined my network! MAC: "MACSTR, MAC2STR(event->mac));

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "AP Started successfully. Connect to me at http://192.168.4.1/");
        lcd_clear();
        lcd_put_str("192.168");
        lcd_goto(1,0);
        lcd_put_str(".4.1/");

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (!is_scanning) {
            ESP_LOGI(TAG, "Wi-Fi STA layer ready.");
            // Removed concurrent internal esp_wifi_connect() to clear race conditions
        }

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (!is_scanning && s_retry_num < MAXIMUM_RETRY) {
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (%d/%d)", s_retry_num, MAXIMUM_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Failed to connect after %d retries. Dropping out.", MAXIMUM_RETRY);

            if (wifi_rst == 1 || old_ssid[0] == 0) {
                current_status = FAILED;
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
                wifi_rst = 0;
                s_retry_num = 0;
                return;
            }
            wifi_rst = 1;
            current_status = FAILED;

            wifi_config_t rollback_config = {0};
            strlcpy((char *)rollback_config.sta.ssid, old_ssid, sizeof(rollback_config.sta.ssid));
            strlcpy((char *)rollback_config.sta.password, old_pass, sizeof(rollback_config.sta.password));
            rollback_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

            esp_wifi_set_config(WIFI_IF_STA, &rollback_config);
            esp_wifi_connect();
        }
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (wifi_rst != 1) {
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                char lcd_buffer[20];
                snprintf(lcd_buffer, sizeof(lcd_buffer), "%.8s", (char*)ap_info.ssid);
                
                lcd_clear();
                lcd_put_str("Connect+");
                lcd_goto(1,0);
                lcd_put_str(lcd_buffer);
            }    
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        
        s_retry_num = 0;
        wifi_rst = 0;

        strlcpy(old_ssid, (char *)current_conf.sta.ssid, sizeof(old_ssid));
        strlcpy(old_pass, (char *)current_conf.sta.password, sizeof(old_pass));

        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Web Server ready! Access at http://" IPSTR "/", IP2STR(&event->ip_info.ip));
        
        uint32_t ip_temp = event->ip_info.ip.addr;
        sprintf(webapp_ip, "%d.%d", (uint8_t)(ip_temp), (uint8_t)(ip_temp >> 8));
        sprintf(webapp_ip2, "%d.%d", (uint8_t)(ip_temp >> 16), (uint8_t)(ip_temp >> 24));

        lcd_clear();
        lcd_put_str(webapp_ip);
        lcd_goto(1,0);
        lcd_put_str(webapp_ip2);
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }
}