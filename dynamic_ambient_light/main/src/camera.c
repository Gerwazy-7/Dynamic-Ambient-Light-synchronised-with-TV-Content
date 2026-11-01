#include "camera.h"
#include "esp_camera.h"
#include "esp_err.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "sensor.h"

bool init_raw;
bool init_jpeg;
framesize_t framesize;

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

esp_err_t init_camera_jpeg(void)
{
    esp_camera_deinit();

    camera_config_t config = {0};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 12000000;

    config.pixel_format = PIXFORMAT_JPEG;

    config.frame_size = FRAMESIZE_QVGA;
    framesize = config.frame_size;

    config.jpeg_quality = 18;

    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    
    sensor_t *s = esp_camera_sensor_get();
    /*s->set_ae_level(s, -2);        
    s->set_gainceiling(s, GAINCEILING_2X);
    s->set_brightness(s, -1);      
    s->set_saturation(s, 1);*/
    s->set_vflip(s, 1);
    if (err != ESP_OK)
    {
        ESP_LOGE("CAMERA", "Błąd w trakcie inicjalizacji kamery (JPEG)");
        return err;
    }

    init_raw = false;
    init_jpeg = true;

    apply_saved_camera_settings();

    ESP_LOGI("CAMERA", "Pomyślna inicjalizacja kamery (JPEG)");
    return ESP_OK;

}

esp_err_t init_camera_raw(void)
{
    esp_camera_deinit();

    camera_config_t config = {0};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;

    config.frame_size = FRAMESIZE_QVGA;
    framesize = config.frame_size;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    
    sensor_t *s = esp_camera_sensor_get();
    int r = s->set_pll(s, 0, 48, 1, 1, 0, 1, 1, 4);
    if (r != 0) ESP_LOGW ("CAMERA", "Błąd set_pll: %d", r);
    ESP_LOGW("CAMERA", "set_pll -> %d", r);
    
    /*s->set_ae_level(s, -2);        // -2..2, najciemniej
    s->set_gainceiling(s, GAINCEILING_2X);
    s->set_brightness(s, -1);      // -2..2
    s->set_saturation(s, 1);       // -2..2, delikatny boost już w czujniku */
    s->set_vflip(s, 1);
     
    if (err != ESP_OK)
    {
        ESP_LOGE("CAMERA", "Błąd w trakcie inicjalizacji kamery (RAW)");
        return err;
    }
    
    init_jpeg = false;
    init_raw = true;

    apply_saved_camera_settings();

    ESP_LOGI("CAMERA", "Pomyślna inicjalizacja kamery (RAW)");
    return ESP_OK;

}
