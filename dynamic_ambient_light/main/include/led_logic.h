#ifndef LED_LOGIC_H
#define LED_LOGIC_H

#include <stdint.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
// dla zmiennej framesize
#include "camera.h"

#define VERTICAL_DIODES 13
#define HORIZONTAL_DIODES 20

#define SAMPLES_VERTICAL 3
#define SAMPLES_HORIZONTAL 4
#define SAMPLES_PER_ZONE (SAMPLES_VERTICAL * SAMPLES_HORIZONTAL)
#define ZONES_NUMBER (VERTICAL_DIODES*2 + HORIZONTAL_DIODES*2)

typedef struct
{
    // [0] - X
    // [1] - Y
    uint16_t bottom[HORIZONTAL_DIODES][2];
    uint16_t up[HORIZONTAL_DIODES][2];
    uint16_t left[VERTICAL_DIODES][2];
    uint16_t right[VERTICAL_DIODES][2];
} led_struct;

typedef struct
{
    uint32_t offsets[SAMPLES_PER_ZONE];
} led_zone_lut;

typedef struct
{
    led_zone_lut zones[ZONES_NUMBER];
} led_lut;

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_struct;

typedef struct
{
    rgb_struct rgb[ZONES_NUMBER];
} output_rgb_struct;


esp_err_t calculate_led_points(led_struct *led_points);

esp_err_t calculate_zones_LUT(led_struct *led_points, led_lut *out_lut);

esp_err_t calculate_colours (led_lut *out_lut, camera_fb_t* fb, output_rgb_struct *results);


#endif // LED_LOGIC_H