#ifndef CAMERA_H
#define CAMERA_H

#include "esp_err.h"
#include "esp_camera.h"
#include "sensor.h"

// ESP pin definition
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5

#define Y9_GPIO_NUM 16
#define Y8_GPIO_NUM 17
#define Y7_GPIO_NUM 18
#define Y6_GPIO_NUM 12
#define Y5_GPIO_NUM 10
#define Y4_GPIO_NUM 8
#define Y3_GPIO_NUM 9
#define Y2_GPIO_NUM 11

#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

extern bool init_raw;
extern bool init_jpeg;
extern framesize_t framesize;

/**
 * 
 */
esp_err_t init_camera_jpeg(void);

esp_err_t init_camera_raw(void);

#endif // CAMERA_H