#ifndef LED_ESP_NOW_H
#define LED_ESP_NOW_H

#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_err.h"
#include <string.h> 
#include "esp_wifi_types_generic.h"
#include "led_logic.h"

esp_err_t esp_now_initialization(void);

esp_err_t send_led_colours(const output_rgb_struct *results);

#endif // LED_ESP_NOW_H