#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_netif.h"
#include "esp_wifi.h"

#include "cJSON.h"

#include "driver/gpio.h"

#include "atm90e26.h"
#include "lcd.h"
#include "wifi.h"
#include "server.h"
#include "socket.h"
#include "my_tasks.h"

#define SEND_INTERVAL_MS 1000

static const char *TAG = "main";

// Shared data structure
atm90e26_data_t meter_data;


void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_wifi_restore();

    for (int i = 0; i < MAX_WS_CLIENTS; i++) s_client_fds[i] = -1;

    gpio_set_pull_mode(15, GPIO_FLOATING);
    gpio_set_drive_capability(15, GPIO_DRIVE_CAP_3);

    lcd_init();
    lcd_put_str("Starting");

    wifi_init();

    spi_init();

    

    //sd_card_mount();

    esp_err_t ret2 = atm90e26_init(SPI2_HOST);
    if (ret2 != ESP_OK) {
        ESP_LOGE(TAG, "ATM90E26 init neuspesan! (%s)", esp_err_to_name(ret2));
    }

    // Launch the background task for metering and broadcasting
    start_meter_task();

    start_web_server(); // 5 task priority


    // Main task stays alive for system monitoring or heartbeats
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}