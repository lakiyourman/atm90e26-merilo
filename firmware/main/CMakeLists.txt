idf_component_register(SRCS "main.c" 
                            "src/lcd.c" 
                            "src/atm90e26.c"
                            "src/cJSON.c"
                            "src/wifi.c"
                            "src/server.c"
                            "src/socket.c"
                            "src/my_tasks.c"
                    EMBED_FILES "index.html"
                    INCLUDE_DIRS "." "include"
                    REQUIRES
                        esp_driver_spi
                        esp_driver_gpio
                        esp_wifi
                        esp_event
                        esp_netif
                        nvs_flash
                        esp_http_server
                        esp_hw_support
                        freertos
                        fatfs 
                        sdmmc
)