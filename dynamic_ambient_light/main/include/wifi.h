#ifndef WIFI_H
#define WIFI_H

#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "esp_mac.h"

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void init_wifi_ap_mode(void);

#endif //WIFI_H