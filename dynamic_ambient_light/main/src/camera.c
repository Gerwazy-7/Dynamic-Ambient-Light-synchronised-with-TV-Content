#include "camera.h"
#include "esp_camera.h"
#include "esp_err.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "sensor.h"
#include <stdint.h>
#include <stdio.h>

bool init_raw;
bool init_jpeg;
framesize_t framesize = FRAME_RESOLUTION;

static SemaphoreHandle_t s_camera_sem = NULL;
static SemaphoreHandle_t s_sensor_mtx = NULL;
static bool s_cam_ready = false;

void camera_manager_init(void)
{
    s_camera_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(s_camera_sem);
    s_sensor_mtx = xSemaphoreCreateMutex();
}

bool camera_acquire(uint32_t timeout_ms)
{
    if (s_camera_sem == NULL)
    {
        return false;
    }
    return xSemaphoreTake(s_camera_sem, timeout_ms) == pdTRUE;
}

void camera_release(void)
{
    if (s_sensor_mtx)
    {
        xSemaphoreTake(s_sensor_mtx, portMAX_DELAY);
    }
    s_cam_ready = false;
    esp_camera_deinit();
    init_raw = false;
    init_jpeg = false;
    if (s_sensor_mtx)
    {
        xSemaphoreGive(s_sensor_mtx);
    }
    if (s_sensor_mtx)
    {
        xSemaphoreGive(s_camera_sem);
    }
}

void apply_saved_camera_settings(void)
{
    FILE *f = fopen("/web/cam_settings.json", "r");
    if (f == NULL) {
        ESP_LOGW("CAMERA", "Brak pliku /web/cam_settings.json - uzywam ustawien fabrycznych");
        return;
    }

    char buf[128];
    if (fgets(buf, sizeof(buf), f) != NULL) {
        int wb = 1, ae = 0, br = 0, co = 0, sa = 0;
        int parsed = sscanf(buf, " { \"whitebal\" : %d , \"ae_level\" : %d , \"brightness\" : %d , \"contrast\" : %d , \"saturation\" : %d } ",
                            &wb, &ae, &br, &co, &sa);
        if (parsed == 5) {
            sensor_t *s = esp_camera_sensor_get();
            if (s != NULL) {
                s->set_whitebal(s, wb);
                s->set_ae_level(s, ae);
                s->set_brightness(s, br);
                s->set_contrast(s, co);
                s->set_saturation(s, sa);
                ESP_LOGI("CAMERA", "Wczytano i zastosowano ustawienia sensora z flash!");
            }
        }
    }
    fclose(f);
}

esp_err_t camera_apply_settings_live(int wb, int ae, int br, int co, int sa)
{
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_sensor_mtx, portMAX_DELAY);

    if (s_cam_ready)
    {
        sensor_t *s = esp_camera_sensor_get();
        if (s != NULL)
        {
            s->set_whitebal(s, wb);
            s->set_ae_level(s, ae);
            s->set_brightness(s, br);
            s->set_contrast(s, co);
            s->set_saturation(s, sa);
            ret = ESP_OK;
        }
    }
    xSemaphoreGive(s_sensor_mtx);
    return ret;
}

static void fill_common_config(camera_config_t *config)
{
    config->ledc_channel = LEDC_CHANNEL_0;
    config->ledc_timer = LEDC_TIMER_0;
    config->pin_d0 = Y2_GPIO_NUM;
    config->pin_d1 = Y3_GPIO_NUM;
    config->pin_d2 = Y4_GPIO_NUM;
    config->pin_d3 = Y5_GPIO_NUM;
    config->pin_d4 = Y6_GPIO_NUM;
    config->pin_d5 = Y7_GPIO_NUM;
    config->pin_d6 = Y8_GPIO_NUM;
    config->pin_d7 = Y9_GPIO_NUM;
    config->pin_xclk = XCLK_GPIO_NUM;
    config->pin_pclk = PCLK_GPIO_NUM;
    config->pin_vsync = VSYNC_GPIO_NUM;
    config->pin_href = HREF_GPIO_NUM;
    config->pin_sccb_sda = SIOD_GPIO_NUM;
    config->pin_sccb_scl = SIOC_GPIO_NUM;
    config->pin_pwdn = PWDN_GPIO_NUM;
    config->pin_reset = RESET_GPIO_NUM;
    config->frame_size = FRAME_RESOLUTION;
    framesize = config->frame_size;

    config->fb_count = 2;
    config->fb_location = CAMERA_FB_IN_PSRAM;
    config->grab_mode = CAMERA_GRAB_LATEST;
}

esp_err_t init_camera_jpeg(void)
{
    xSemaphoreTake(s_sensor_mtx, portMAX_DELAY);    
    s_cam_ready = false;
    
    if (init_raw || init_jpeg)
    {
        esp_camera_deinit();
    }

    camera_config_t config = {0};
    fill_common_config(&config);
    config.pixel_format = PIXFORMAT_JPEG;
    config.jpeg_quality = 18;
    config.xclk_freq_hz = 12000000;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE("CAMERA", "Error while initializing camera (JPEG)");
        xSemaphoreGive(s_sensor_mtx);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);
    apply_saved_camera_settings();

    init_raw = false;
    init_jpeg = true;
    s_cam_ready = true;

    ESP_LOGI("CAMERA", "Successfull camera initialization (JPEG)");
    xSemaphoreGive(s_sensor_mtx);
    return ESP_OK;
}

esp_err_t init_camera_raw(void)
{
    xSemaphoreTake(s_sensor_mtx, portMAX_DELAY);
    s_cam_ready = false;
    if (init_raw || init_jpeg)
    {
        esp_camera_deinit();
    }

    camera_config_t config = {0};
    fill_common_config(&config);
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE("CAMERA", "Error while initializing camera (RAW)");
        xSemaphoreGive(s_sensor_mtx);
        return err;
    }


    sensor_t *s = esp_camera_sensor_get();
    int r = s->set_pll(s, 0, 48, 1, 1, 0, 1, 1, 4);
    if (r != 0)
    {
        ESP_LOGW ("CAMERA", "Błąd set_pll: %d", r);
    }
    s->set_vflip(s, 1);
    apply_saved_camera_settings();

    init_jpeg = false;
    init_raw = true;
    s_cam_ready = true;

    ESP_LOGI("CAMERA", "Successfull camera initialization (RAW)");
    xSemaphoreGive(s_sensor_mtx);
    return ESP_OK;

}
