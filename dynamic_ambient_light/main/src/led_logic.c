#include "led_logic.h"
#include "camera.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sensor.h"
#include <math.h>
#include <stdint.h>
#include <sys/types.h>

#define EMA_coef 0.3
#define R_GAIN 1.5f

esp_err_t calculate_led_points(led_struct *led_points)
{
    int res_width = resolution[framesize].width;
    int res_height = resolution[framesize].height;

    float coordinates[4][2] = 
    {
        {5.0f, 95.0f},
        {95.0f, 95.0f},
        {95.0f, 5.0f},
        {5.0f, 5.0f}
    };
    FILE *f = fopen("/web/markers.json", "r");
    if (f == NULL)
    {
        ESP_LOGE("LED_CALCULATE", "Error while reading marker's position from file");
        return ESP_FAIL;
    }
    int parsed = fscanf(f, "[[%f,%f],[%f,%f],[%f,%f],[%f,%f]]",
                        &coordinates[0][0], &coordinates[0][1],
                        &coordinates[1][0], &coordinates[1][1],
                        &coordinates[2][0], &coordinates[2][1],
                        &coordinates[3][0], &coordinates[3][1]);
    fclose(f);
    // LINEAR INTERPOLATION

    if (parsed != 8)
    {
        ESP_LOGE("LED_CALCULATE","Error while parsing JSON data");
        return ESP_FAIL;
    }

    for (int i = 0; i < HORIZONTAL_DIODES; i++)
    {
        float coef = (float)(i)/(float)(HORIZONTAL_DIODES-1);

        led_points->bottom[i][0] = floor((coordinates[0][0] + coef * (coordinates[1][0] - coordinates[0][0])) / 100 * (res_width - 1));
        led_points->bottom[i][1] = floor((coordinates[0][1] + coef * (coordinates[1][1] - coordinates[0][1])) / 100 * (res_height - 1));

        led_points->up[i][0] = floor((coordinates[2][0] + coef * (coordinates[3][0] - coordinates[2][0])) / 100 * (res_width - 1));
        led_points->up[i][1] = floor((coordinates[2][1] + coef * (coordinates[3][1] - coordinates[2][1])) / 100 * (res_height - 1));
    }

    for (int i = 0; i < VERTICAL_DIODES; i++)
    {
        float coef = (float)(i)/(float)(VERTICAL_DIODES-1);

        led_points->left[i][0] = floor((coordinates[3][0] + coef * (coordinates[0][0] - coordinates[3][0])) / 100 * (res_width - 1));
        led_points->left[i][1] = floor((coordinates[3][1] + coef * (coordinates[0][1] - coordinates[3][1])) / 100 * (res_height - 1));

        led_points->right[i][0] = floor((coordinates[1][0] + coef * (coordinates[2][0] - coordinates[1][0])) / 100 * (res_width - 1));
        led_points->right[i][1] = floor((coordinates[1][1] + coef * (coordinates[2][1] - coordinates[1][1])) / 100 * (res_height - 1));
    }  
    //ESP_LOGI("LED_CALCULATE", "Calculated led points successfully!");
    return ESP_OK;
}

esp_err_t calculate_zones_LUT(led_struct *led_points, led_lut *out_lut)
{
    if (led_points == NULL || out_lut == NULL)
    {
        ESP_LOGE("LUT","Błąd tablic algorytmu obliczania stref");
        return ESP_ERR_INVALID_ARG;
    }

    size_t total_elements = ZONES_NUMBER * SAMPLES_PER_ZONE;
    
    uint16_t *x_coordinates = (uint16_t *)malloc(total_elements * sizeof(uint16_t));
    uint16_t *y_coordinates = (uint16_t *)malloc(total_elements * sizeof(uint16_t));

    if(x_coordinates == NULL || y_coordinates == NULL)
    {
        ESP_LOGE("LUT", "Brak wolnego RAM-U na tablice tymczasowe");
        if(x_coordinates) {free(x_coordinates);}
        if(y_coordinates) {free(y_coordinates);}
        return ESP_ERR_NO_MEM;
    }
    uint16_t index_x = 0;
    uint16_t index_y = 0;

    uint16_t left_side_zone_width = 0;
    uint16_t right_side_zone_width = 0;

    // ================== X COORDINATES ==================//
    // ========= BOTTOM ROW ========= //
    for (int i = 0; i < HORIZONTAL_DIODES; i++)
    {
        uint16_t left_boundary;
        uint16_t right_boundary;
        uint16_t bottom_boundary = led_points->left[VERTICAL_DIODES-1][1];
        uint16_t top_boundary = (led_points->left[VERTICAL_DIODES-1][1] + led_points->left[VERTICAL_DIODES-2][1]) / 2;

        // Left boundary
        if (i == 0)
        {
            left_boundary = led_points->bottom[0][0];   
        }
        else
        {
            left_boundary = (led_points->bottom[i-1][0] + led_points->bottom[i][0]) / 2;
        }

        // Right boundary
        if (i == HORIZONTAL_DIODES - 1)
        {
            right_boundary = led_points->bottom[i][0];
        }
        else
        {
            right_boundary = (led_points->bottom[i][0] + led_points->bottom[i+1][0]) / 2;
        }
        uint16_t start_x = left_boundary + 2;
        uint16_t end_x = right_boundary - 2;
        uint16_t start_y = top_boundary + 2;
        uint16_t end_y = bottom_boundary - 2;

        uint16_t zone_width =(end_x > start_x) ? (end_x - start_x) : 0;
        uint16_t zone_height =(end_y > start_y) ? (end_y - start_y) : 0;

        //ESP_LOGW("LUT_BOTTOM", "R[%d]: y %d..%d, h=%d", i, start_y, end_y, zone_height);

        if (i == 1) {left_side_zone_width = zone_width;}
        if (i == HORIZONTAL_DIODES - 2) {right_side_zone_width = zone_width;}

        float distance_to_point_x = (float)zone_width / (SAMPLES_HORIZONTAL - 1);
        float distance_to_point_y = (float)zone_height  / (SAMPLES_VERTICAL - 1);

        for (int j = 0; j < SAMPLES_VERTICAL; j++)
        {    
            uint16_t current_y = start_y + (int)(j * distance_to_point_y);
            for (int k = 0; k < SAMPLES_HORIZONTAL; k++)
            {
                x_coordinates[index_x++] = start_x + (int)(k * distance_to_point_x);
                y_coordinates[index_y++] = current_y;
            }
        }
    }

    // ======= Right column ======= //
    for (int i = 0; i < VERTICAL_DIODES; i++)
    {
        int end_x = (int)led_points->right[i][0] - 2;
        int start_x = end_x - right_side_zone_width;

        uint16_t bottom_boundary;
        uint16_t top_boundary;
        if (i == 0)
        {
            bottom_boundary = led_points->right[0][1];
        }
        else
        {
            bottom_boundary = (led_points->right[i-1][1] + led_points->right[i][1]) / 2;
        }
        
        if (i == VERTICAL_DIODES - 1)
        {
            top_boundary = led_points->right[i][1];
        }
        else 
        {
            top_boundary = (led_points->right[i][1] + led_points->right[i+1][1]) / 2;
        }
        uint16_t start_y = top_boundary + 2;
        uint16_t end_y = bottom_boundary - 2;
        
        uint16_t zone_height = (end_y > start_y) ? (end_y - start_y) : 0;

        //ESP_LOGW("LUT_RIGHT", "R[%d]: y %d..%d, h=%d", i, start_y, end_y, zone_height);

        float distance_to_point_x = (float)right_side_zone_width / (SAMPLES_HORIZONTAL - 1);
        float distance_to_point_y = (float)zone_height / (SAMPLES_VERTICAL - 1);   
        for (int j = 0; j < SAMPLES_VERTICAL; j++)
        {
            uint16_t current_y = start_y + (int)(j * distance_to_point_y);
            for (int k = 0; k < SAMPLES_HORIZONTAL; k++)
            {
                x_coordinates[index_x++] = start_x + (int)(k * distance_to_point_x);
                y_coordinates[index_y++] = current_y;
            }
        }
    }

    // ========= UPPER ROW ========= //
    for (int i = 0; i < HORIZONTAL_DIODES; i++)
    {
        uint16_t left_boundary;
        uint16_t right_boundary;
        uint16_t top_boundary = led_points->left[0][1];
        uint16_t bottom_boundary = (led_points->left[0][1] + led_points->left[1][1]) / 2;

        // Right boundary
        if (i == 0)
        {
            right_boundary = led_points->up[0][0];
        }
        else
        {
            right_boundary = (led_points->up[i-1][0] + led_points->up[i][0]) / 2;
        }

        // left boundary
        if (i == HORIZONTAL_DIODES - 1)
        {
            left_boundary = led_points->up[i][0];
        }
        else
        {
            left_boundary = (led_points->up[i][0] + led_points->up[i+1][0]) / 2;
        }
        uint16_t start_x = left_boundary + 2;
        uint16_t end_x = right_boundary - 2;
        uint16_t start_y = top_boundary + 2;
        uint16_t end_y = bottom_boundary - 2;

        uint16_t zone_width = (end_x > start_x) ? (end_x - start_x) : 0;
        uint16_t zone_height = (end_y > start_y) ? (end_y - start_y) : 0;

        float distance_to_point_x = (float)zone_width / (SAMPLES_HORIZONTAL - 1);
        float distance_to_point_y = (float)zone_height / (SAMPLES_VERTICAL - 1);

        //ESP_LOGW("LUT_UP", "R[%d]: y %d..%d, h=%d", i, start_y, end_y, zone_height);
        for (int j = 0; j < SAMPLES_VERTICAL; j++)
        {
            uint16_t current_y = start_y + (int)(j * distance_to_point_y);
            for (int k = 0; k < SAMPLES_HORIZONTAL; k++)
            {
                x_coordinates[index_x++] = start_x + (int)(k * distance_to_point_x);
                y_coordinates[index_y++] = current_y;
            }
        }
    }
    // ======= Left column ======= //
    for (int i = 0; i < VERTICAL_DIODES; i++)
    {
        uint16_t start_x = led_points->left[i][0] + 2;

        uint16_t bottom_boundary = 0;
        uint16_t top_boundary = 0;

       if (i == 0)
       {
            top_boundary = led_points->left[i][1];
       }
       else
       {
            top_boundary = (led_points->left[i-1][1] + led_points->left[i][1]) / 2;
       }
 
       if (i == VERTICAL_DIODES - 1)
       {
            bottom_boundary = led_points->left[i][1];
       }
       else
       {
            bottom_boundary = (led_points->left[i+1][1] + led_points->left[i][1]) / 2;
       }

        uint16_t start_y = top_boundary + 2;
        uint16_t end_y = bottom_boundary - 2;
        uint16_t zone_height = (end_y > start_y) ? (end_y - start_y) : 0;

        //ESP_LOGW("LUT_LEFT", "R[%d]: y %d..%d, h=%d", i, start_y, end_y, zone_height);

        float distance_to_point_x = (float)left_side_zone_width / (SAMPLES_HORIZONTAL - 1);
        float distance_to_point_y = (float)zone_height / (SAMPLES_VERTICAL - 1);

        for (int j = 0; j < SAMPLES_VERTICAL; j++)
        {
            uint16_t current_y = start_y + (int)(j * distance_to_point_y);
            for (int k = 0; k < SAMPLES_HORIZONTAL; k++)
            {
                x_coordinates[index_x++] = start_x + (int)(k * distance_to_point_x);
                y_coordinates[index_y++] = current_y;
            }
        }
    }
    int total_zones = ZONES_NUMBER;
    int sample_counter = 0;
    int clamped = 0;
    // ======== OUT LUT ======== //
    for (int i = 0; i < total_zones; i++)
    {
        for (int j = 0; j < SAMPLES_PER_ZONE; j++)
        {
            uint32_t x = x_coordinates[sample_counter];
            uint32_t y = y_coordinates[sample_counter];
            if (x >= resolution[framesize].width || y >= resolution[framesize].height) 
            {
                //ESP_LOGW("LUT", "Clamp: strefa %d, probka %d, x=%lu y=%lu", i, j, (unsigned long)x, (unsigned long)y);
                clamped++;
                if (x >= resolution[framesize].width)
                {
                    x = resolution[framesize].width - 1;
                }
                if (y >= resolution[framesize].height)
                {
                    y = resolution[framesize].height - 1;
                }
            }
            out_lut->zones[i].offsets[j] = (uint32_t)(y * resolution[framesize].width + x);
            sample_counter++;
        }
    }
    //ESP_LOGI("LUT", "Przyciete probki: %d / %d", clamped, ZONES_NUMBER * SAMPLES_PER_ZONE);

    free(x_coordinates);
    free(y_coordinates);
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

            if (max_val < 40)
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