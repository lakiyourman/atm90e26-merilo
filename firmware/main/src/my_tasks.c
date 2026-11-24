#include "my_tasks.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "cJSON.h"
#include "atm90e26.h"
#include "socket.h"
#include "driver/spi_master.h"
#include "esp_vfs_fat.h"

static const char *TAG = "METER_TASK";
#define SEND_INTERVAL_MS 1000

#define PIN_MISO 15 // 15
#define PIN_MOSI 16 //16
#define PIN_SCLK 17 //17

#define PIN_CS_SD  21 //21 final


static QueueHandle_t sd_json_queue = NULL; // Queue to send the final JSON string pointer to the SD task
static sdmmc_card_t *card = NULL; // This holds the card info after mounting

void spi_init(void){
    spi_bus_config_t bus_cfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret4 = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret4 != ESP_OK) {
        ESP_LOGE("SPI", "Failed to initialize SPI bus");
        return;
    }
    else ESP_LOGI("SPI", "SPI bus intializes successfully!");
    vTaskDelay(pdMS_TO_TICKS(500));
}

sdmmc_card_t* get_sd_card(void){
    return card;
}

void sd_card_mount(void){

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST; // Your GPIO 8, 9, 10 bus

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_CS_SD;
    slot_config.host_id = SPI2_HOST;

    // NEW: This now takes 5 arguments
    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sd", &host, &slot_config, &mount_config, &card); //wrapper for spi_add...
    if (ret != ESP_OK) {
        ESP_LOGE("SD_CARD", "Failed to mount SD card (%s)", esp_err_to_name(ret));
        card=NULL;
    }

}

/**
 * Task: SD Card Logger (Consumer)
 * This task waits for a JSON string pointer and writes it to SD.
 */
void sd_logger_task(void *pvParameters) {
    char *json_to_save;
    const char *path = "/sd/history.json";
    ESP_LOGI(TAG, "SD Logger Task Started.");
    
    while (1) {
        if (xQueueReceive(sd_json_queue, &json_to_save, portMAX_DELAY)) {
            if (get_sd_card() != NULL) {    
                FILE *f = fopen(path, "a");
                if (f == NULL) {
                    ESP_LOGE(TAG, "Failed to open file for appending");
                } else {
                    // Write the JSON string plus a newline for easier parsing later
                    fprintf(f, "%s\n", json_to_save);
                    fclose(f);
                    // ESP_LOGD(TAG, "SD: Data logged successfully");
                }
            }

            // Always free the string we duplicated in the meter task
            free(json_to_save);
        }
    }
}

/**
 * Task: Metering and JSON Broadcast (Producer)
 */
void meter_logger_task(void *pvParameters) {
    atm90e26_data_t meter_data;
    while (1) {
        esp_err_t ret = atm90e26_read_all(&meter_data);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Greška pri čitanju ATM90E26");
        } else {
            /* 1. Ispis na serijski terminal - UNCHANGED */
            ESP_LOGI(TAG,
                "U=%.2fV  I=%.3fA  F=%.2fHz  P=%.1fW  PF=%.3f  E=%.1fWh  Status=0x%04X",
                meter_data.voltage_rms,
                meter_data.current_rms,
                meter_data.frequency,
                meter_data.active_power,
                meter_data.power_factor,
                meter_data.active_energy,
                meter_data.sys_status);

            /* 2. Formiranje JSON poruke - UNCHANGED */
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "voltage",      meter_data.voltage_rms);
            cJSON_AddNumberToObject(root, "current",      meter_data.current_rms);
            cJSON_AddNumberToObject(root, "frequency",    meter_data.frequency);
            cJSON_AddNumberToObject(root, "power",        meter_data.active_power);
            cJSON_AddNumberToObject(root, "power_factor", meter_data.power_factor);
            cJSON_AddNumberToObject(root, "energy",       meter_data.active_energy);
            cJSON_AddNumberToObject(root, "status",       meter_data.sys_status);

            char *json_str = cJSON_PrintUnformatted(root);
            if (json_str) {
                // 3. Send to Web Interface
                websocket_broadcast_json(json_str);

                // 4. Send the EXACT same string to SD task
                if (sd_json_queue != NULL) {
                    // We duplicate the string so the SD task can own its own copy
                    char *sd_copy = strdup(json_str);
                    if (sd_copy) {
                        if (xQueueSend(sd_json_queue, &sd_copy, 0) != pdPASS) {
                            free(sd_copy); // Queue full, discard to prevent leak
                        }
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(5)); 
                free(json_str);
            }
            cJSON_Delete(root);
        }

        vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS));
    }
}

void start_meter_task(void) {
    // Queue stores POINTERS to strings (char*)
    sd_json_queue = xQueueCreate(10, sizeof(char *));

    // Start tasks
    xTaskCreate(meter_logger_task, "meter_task", 4096, NULL, 5, NULL);
    //xTaskCreate(sd_logger_task, "sd_task", 4096, NULL, 4, NULL);
}