idf_component_register(SRCS "main.c"
                            "src/camera.c"
                            "src/server.c"
                            "src/littlefs.c"
                            "src/wifi.c"
                            "src/led_logic.c"
                            "src/led_esp_now.c"
                        INCLUDE_DIRS "." "include"
                        REQUIRES 
                                nvs_flash
                                esp_wifi
                                esp_event
                                esp_netif
                                esp_http_server
                                esp32-camera
                                esp_timer)
littlefs_create_partition_image(storage ../web FLASH_IN_PROJECT)