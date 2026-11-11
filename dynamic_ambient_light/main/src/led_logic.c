#include "led_logic.h"
#include "camera.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "sensor.h"
#include <math.h>
#include <stdint.h>
#include <sys/types.h>

#define EMA_coef 0.3
#define R_GAIN 1.8f
#define ZONE_MARGIN 9

float calculate_determinant(float a, float b, float c, float d)
{
    return a * d - b * c;
}

esp_err_t calculate_map(zones_map *points_map)
{
    int res_width = resolution[framesize].width;
    int res_height = resolution[framesize].height;

    float coordinates[4][2] = 
    {
        {20.0f, 80.0f},
        {80.0f, 80.0f},
        {80.0f, 20.0f},
        {20.0f, 20.0f}
    };

    FILE *file = fopen("/web/markers.json", "r");
    if (file == NULL)
    {
        ESP_LOGE("LED_CALCULATE", "Error while reading marker's position from file");
        return ESP_FAIL;
    }
    int parsed = fscanf(file, "[[%f,%f],[%f,%f],[%f,%f],[%f,%f]]",
                        &coordinates[0][0], //x0   left-down
                        &coordinates[0][1], //y0   
                        &coordinates[1][0], //x1   right-down
                        &coordinates[1][1], //y1
                        &coordinates[2][0], //x2   right-up
                        &coordinates[2][1], //y2
                        &coordinates[3][0], //x3   left-up
                        &coordinates[3][1]);//y3
    fclose(file);

    if (parsed != 8)
    {
        ESP_LOGE("LED_CALCULATE","Error while parsing JSON data");
        return ESP_FAIL;
    }

    for (int i = 0; i < 4; i++)
    {
        coordinates[i][0] = coordinates[i][0] * res_width / 100.0f;
        coordinates[i][1] = coordinates[i][1] * res_height / 100.0f;
    }

    float x0 = coordinates[0][0];
    float y0 = coordinates[0][1];
    float x1 = coordinates[1][0];
    float y1 = coordinates[1][1];
    float x2 = coordinates[2][0];
    float y2 = coordinates[2][1];
    float x3 = coordinates[3][0];
    float y3 = coordinates[3][1];

    float d_x1 = x1 - x2;
    float d_x2 = x3 - x2;
    float d_y1 = y1 - y2;
    float d_y2 = y3 - y2;

    float sigma_x = x0 - x1 + x2 - x3;
    float sigma_y = y0 - y1 + y2 - y3;

    float denominator = calculate_determinant(d_x1, d_x2, d_y1, d_y2);
    if (fabsf(denominator) < 1e-6f)
    {
        ESP_LOGE("LED_CALCULATE", "Degenerate quadrilateral (det == 0)");
        return ESP_FAIL;
    }
    float g = calculate_determinant(sigma_x, d_x2, sigma_y,d_y2) / denominator;
    float h = calculate_determinant(d_x1, sigma_x, d_y1,sigma_y) / denominator;
    float a = x1 - x0 + g * x1;
    float b = x3 - x0 + h * x3;
    float c = x0;
    float d = y1 - y0 + g * y1;
    float e = y3 - y0 + h * y3;
    float f = y0;

    float t_horizontal[LED_POINTS_X];
    float t_vertical[LED_POINTS_Y];

    for (int i = 0; i < LED_POINTS_X; i++)
    {
        if (HORIZONTAL_DIODES > 1)
        {
            t_horizontal[i] = (float)i / (LED_POINTS_X - 1);
        }
        else
        {
            t_horizontal[i] = 0.0f;
        }
    }

    for (int i = 0; i < LED_POINTS_Y; i++)
    {
        if (VERTICAL_DIODES > 1)
        {
            t_vertical[i] = (float)i / (LED_POINTS_Y- 1);
        }
        else
        {
            t_vertical[i] = 0.0f;
        }
    }

    // bottom
    for (int i = 0; i < LED_POINTS_X; i++)
    {
        for (int j = 0; j < SAMPLES_VERTICAL; j++)
        {
            float temp_denominator = g * t_horizontal[i] + h * t_vertical[j] + 1;
            points_map->bottom[i][j][0] = (a * t_horizontal[i] + b * t_vertical[j] + c) / temp_denominator;
            points_map->bottom[i][j][1] = (d * t_horizontal[i] + e * t_vertical[j] + f) / temp_denominator;
        }
    }

    // right
    for (int i = 0; i < LED_POINTS_Y; i++)
    {
        for (int j = 0; j < SAMPLES_HORIZONTAL; j++)
        {
            float temp_denominator = g * t_horizontal[LED_POINTS_X - 1 - j] + h * t_vertical[i] + 1;
            points_map->right[i][j][0] = (a * t_horizontal[LED_POINTS_X - 1 - j] + b * t_vertical[i] + c) / temp_denominator;
            points_map->right[i][j][1] = (d * t_horizontal[LED_POINTS_X - 1 - j] + e * t_vertical[i] + f) / temp_denominator;
        }
    }

    // top
    for (int i = 0; i < LED_POINTS_X; i++)
    {
        for (int j = 0; j < SAMPLES_VERTICAL; j++)
        {
            float temp_denominator = g * t_horizontal[LED_POINTS_X - 1 - i] + h * t_vertical[LED_POINTS_Y - 1 - j] + 1;
            points_map->top[i][j][0] = (a * t_horizontal[LED_POINTS_X - 1 - i] + b * t_vertical[LED_POINTS_Y - 1 - j] + c) / temp_denominator;
            points_map->top[i][j][1] = (d * t_horizontal[LED_POINTS_X - 1 - i] + e * t_vertical[LED_POINTS_Y - 1 - j] + f) / temp_denominator;
        }
    }


    // left
    for (int i = 0; i < LED_POINTS_Y; i++)
    {
        for (int j = 0; j < SAMPLES_HORIZONTAL; j++)
        {
            float temp_denominator = g * t_horizontal[j] + h * t_vertical[LED_POINTS_Y - 1 - i] + 1;
            points_map->left[i][j][0] = (a * t_horizontal[j] + b * t_vertical[LED_POINTS_Y - 1 - i] + c) / temp_denominator;
            points_map->left[i][j][1] = (d * t_horizontal[j] + e * t_vertical[LED_POINTS_Y - 1 - i] + f) / temp_denominator;
        }
    }

    ESP_LOGI("MAP", "bottom[0]=(%d,%d) right[0]=(%d,%d) top[0]=(%d,%d) left[0]=(%d,%d)",
    points_map->bottom[0][0][0], points_map->bottom[0][0][1],
    points_map->right[0][0][0],  points_map->right[0][0][1],
    points_map->top[0][0][0],    points_map->top[0][0][1],
    points_map->left[0][0][0],   points_map->left[0][0][1]);

    return ESP_OK;
}

esp_err_t calculate_lut(zones_map *points_map, led_lut *out_lut)
{
    if (points_map == NULL || out_lut == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t width  = resolution[framesize].width;
    uint32_t height = resolution[framesize].height;

    int zone_idx = 0;

    //======= bottom ========//
    for (int d = 0; d < HORIZONTAL_DIODES; d++)
    {
        int sample_idx = 0;

        //wzdłuż krawędzi
        for (int j = 0; j < SAMPLES_HORIZONTAL; j++)
        {
            //wgłąb ekranu
            for (int k = 0; k < SAMPLES_VERTICAL; k++)
            {
                int edge_idx = d * SAMPLES_HORIZONTAL + j;
                uint32_t x = points_map->bottom[edge_idx][k][0];
                uint32_t y = points_map->bottom[edge_idx][k][1];

                if (x >= width)
                {
                    x = width - 1;
                }
                if (y >= height)
                {
                    y = height - 1;
                }

                out_lut->zones[zone_idx].offsets[sample_idx++] = (uint32_t)(y * resolution[framesize].width + x);
            }
        }
        zone_idx++;
    }

    // ======== rigth ======== //
    for (int d = 0; d < VERTICAL_DIODES; d++)
    {
        int sample_idx = 0;

        //wzdłuż krawędzi
        for (int j = 0; j < SAMPLES_VERTICAL; j++)
        {
            //wgłąb ekranu
            for (int k = 0; k < SAMPLES_HORIZONTAL; k++)
            {
                int edge_idx = d * SAMPLES_VERTICAL + j;
                uint32_t x = points_map->right[edge_idx][k][0];
                uint32_t y = points_map->right[edge_idx][k][1];

                if (x >= width)
                {
                    x = width - 1;
                }
                if (y >= height)
                {
                    y = height - 1;
                }

                out_lut->zones[zone_idx].offsets[sample_idx++] = (uint32_t)(y * resolution[framesize].width + x);
            }
        }
        zone_idx++;
    }

    //======= top ========//
    //wzdłuż krawędzi
    for (int d = 0; d < HORIZONTAL_DIODES; d++)
    {
        int sample_idx = 0;

        //wzdłuż krawędzi
        for (int j = 0; j < SAMPLES_HORIZONTAL; j++)
        {
            //wgłąb ekranu
            for (int k = 0; k < SAMPLES_VERTICAL; k++)
            {
                int edge_idx = d * SAMPLES_HORIZONTAL + j;
                uint32_t x = points_map->top[edge_idx][k][0];
                uint32_t y = points_map->top[edge_idx][k][1];

                if (x >= width)
                {
                    x = width - 1;
                }
                if (y >= height)
                {
                    y = height - 1;
                }

                out_lut->zones[zone_idx].offsets[sample_idx++] = (uint32_t)(y * resolution[framesize].width + x);
            }
        }
        zone_idx++;
    }

        // ======== left ======== //
    for (int d = 0; d < VERTICAL_DIODES; d++)
    {
        int sample_idx = 0;

        //wzdłuż krawędzi
        for (int j = 0; j < SAMPLES_VERTICAL; j++)
        {
            //wgłąb ekranu
            for (int k = 0; k < SAMPLES_HORIZONTAL; k++)
            {
                int edge_idx = d * SAMPLES_VERTICAL + j;
                uint32_t x = points_map->left[edge_idx][k][0];
                uint32_t y = points_map->left[edge_idx][k][1];

                if (x >= width)
                {
                    x = width - 1;
                }
                if (y >= height)
                {
                    y = height - 1;
                }

                out_lut->zones[zone_idx].offsets[sample_idx++] = (uint32_t)(y * resolution[framesize].width + x);
            }
        }
        zone_idx++;
    }
    return ESP_OK;
}

static rgb_struct prev_results[ZONES_NUMBER] = {0};

esp_err_t calculate_colours (led_lut *out_lut, camera_fb_t* fb, output_rgb_struct *results)
{
    uint16_t *buf = (uint16_t *)fb->buf;
    rgb_struct temp_rgb = {};

    for (int i = 0; i < ZONES_NUMBER; i++)
    {
        uint32_t total_weight = 0;
        uint32_t r_weighted = 0;
        uint32_t g_weighted = 0;
        uint32_t b_weighted = 0;
        for (int j = 0; j < SAMPLES_PER_ZONE; j++)
        {
            // Dekodowanie RGB565 do RGB888
            uint16_t cur_pixel = __builtin_bswap16(buf[out_lut->zones[i].offsets[j]]);
            //ESP_LOGI("DBG", "raw=0x%04X", buf[out_lut->zones[0].offsets[0]]);
            temp_rgb.r = ((cur_pixel >> 11) & 0x1F) << 3;
            temp_rgb.g = ((cur_pixel >> 5)  & 0x3F) << 2;
            temp_rgb.b = ((cur_pixel)       & 0x1F) << 3;

            
            uint8_t max_val = temp_rgb.r;
            if (temp_rgb.g > max_val)
            {
                max_val = temp_rgb.g;
            }
            if (temp_rgb.b > max_val)
            {
                max_val = temp_rgb.b;
            }

            uint8_t min_val = temp_rgb.r;
            if (temp_rgb.g < min_val)
            {
                min_val = temp_rgb.g;
            }
            if (temp_rgb.b < min_val)
            {
                min_val = temp_rgb.b;
            }

            if (max_val < 50)
            {
                continue;
            }

            uint8_t saturation = max_val - min_val;
            uint32_t weight = (saturation * saturation) / 256 + 1 ;

            r_weighted += temp_rgb.r * weight;
            g_weighted += temp_rgb.g * weight;
            b_weighted += temp_rgb.b * weight;
            total_weight += weight;
        }

        float target_r = 0;
        float target_g = 0;
        float target_b = 0;

        if (total_weight > 0)
        {
            target_r = (float)r_weighted / total_weight;
            target_g = (float)g_weighted / total_weight;
            target_b = (float)b_weighted / total_weight;
        }

        uint8_t filtered_r = (uint8_t)(EMA_coef * target_r + (1-EMA_coef) * prev_results[i].r + 0.5f);
        uint8_t filtered_g = (uint8_t)(EMA_coef * target_g + (1-EMA_coef) * prev_results[i].g + 0.5f);
        uint8_t filtered_b = (uint8_t)(EMA_coef * target_b + (1-EMA_coef) * prev_results[i].b + 0.5f);

        prev_results[i].r = filtered_r;
        prev_results[i].g = filtered_g;
        prev_results[i].b = filtered_b;

        int r_out = (int)(filtered_r * R_GAIN);
        if(r_out > 255)
        {
            r_out = 255;
        }

        results->rgb[i].r = r_out;
        results->rgb[i].g = filtered_g;
        results->rgb[i].b = filtered_b;
    }
    return ESP_OK;
}