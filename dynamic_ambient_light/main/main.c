#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "led_esp_now.h"
#include "littlefs.h"
#include "camera.h"
#include "server.h"
#include "wifi.h"

void app_main(void)
{
    init_raw = false;
    init_jpeg = false;
    lights_on = false;
    
    camera_manager_init();

    init_littleFS();
    init_wifi_ap_mode();
    esp_now_initialization();

    start_webserver();

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}