idf_component_register(SRCS "led_control.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_wifi esp_event esp_netif led_strip)
