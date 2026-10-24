#ifndef SERVER_H
#define SERVER_H

#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "camera.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include <stdint.h>
#include <stdio.h>
#include "esp_event_base.h"

#include "led_logic.h"

// String used as frame boundry identifier
#define PART_BOUNDARY "This_is_boudry_of_frame"

extern volatile bool lights_on;
extern volatile bool is_streaming;

void ambilight_task (void *pvParameters);

esp_err_t file_server_get_handler(httpd_req_t *req);

static esp_err_t save_markers_handler(httpd_req_t *req);

static esp_err_t lights_handler(httpd_req_t *req);

esp_err_t stream_get_handler(httpd_req_t *req);

esp_err_t start_webserver(void);

#endif // SERVER_H