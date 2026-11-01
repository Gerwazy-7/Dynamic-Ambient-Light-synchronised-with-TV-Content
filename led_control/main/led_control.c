#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "led_strip.h"

#define LED_STRIP_GPIO 4
#define HORIZONTAL_DIODES 20
#define VERTICAL_DIODES 13

//NATURAL WHITE
#define RW 255
#define GW 160
#define BW 140

#define ZONES_NUMBER (VERTICAL_DIODES * 2 + HORIZONTAL_DIODES * 2)

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

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
} rgbw_struct;

static led_strip_handle_t led_strip;

static const char *TAG = "ESP-NOW RX";

// function based on https://github.com/BertanT/Arduino-RGBWConverter/blob/main/src/RGBWConverter.cpp
static rgbw_struct rgb_to_rgbw(const rgb_struct *rgb_values)
{  
    rgbw_struct output;
    float wRed = rgb_values->r * (255.0f / RW);
    float wGreen = rgb_values->g * (255.0f / GW);
    float wBlue = rgb_values->b * (255.0f / BW);

    float wMin = wRed;
    if (wGreen < wMin)
    {
        wMin = wGreen;
    }
    if (wBlue < wMin)
    {
        wMin = wBlue;
    }

    if (wMin > 255.0f)
    {
        wMin = 255.0f;
    }
    else if (wMin < 0.0f)
    {
        wMin = 0.0f;
    }

    int r = (int)roundf(rgb_values->r - wMin * (RW / 255.0f));
    int g = (int)roundf(rgb_values->g - wMin * (GW / 255.0f));
    int b = (int)roundf(rgb_values->b - wMin * (BW / 255.0f));

    output.r = (r < 0) ? 0 : (r > 255 ? 255 : (uint8_t)r);
    output.g = (g < 0) ? 0 : (g > 255 ? 255 : (uint8_t)g);
    output.b = (b < 0) ? 0 : (b > 255 ? 255 : (uint8_t)b);
    output.w = (uint8_t)(roundf(wMin));

    return output;
}

// Callback wywoływany w momencie odebrania pakietu
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len == sizeof(output_rgb_struct))
    {
        const output_rgb_struct *frame = (const output_rgb_struct *)data;

        for (int i = 0; i < ZONES_NUMBER; i++)
        {
            rgbw_struct output = rgb_to_rgbw(&frame->rgb[i]);
            //led_strip_set_pixel_rgbw(led_strip, i, output.r, output.g, output.b, output.w);
            led_strip_set_pixel_rgbw(led_strip, i, frame->rgb[i].r, frame->rgb[i].g, frame->rgb[i].b, 0);
        } 
        led_strip_refresh(led_strip);
    }
    //else
}

static void init_leds(void)
{
    led_strip_config_t strip_config =
    {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = ZONES_NUMBER,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRBW,
        .led_model = LED_MODEL_SK6812,
        .flags = 
        {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t rmt_config =
    {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    ESP_LOGI("ESP-NOW RX", "Pasek LED zainicjalizowany na GPIO %d (%d diod)", LED_STRIP_GPIO, ZONES_NUMBER);
}

void app_main(void)
{
    // 1. Inicjalizacja pamięci NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    init_leds();
    
    // 2. Podstawowa inicjalizacja sieci
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Konfiguracja Wi-Fi w trybie Station
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 4. Wyłączenie usypiania Wi-Fi (Power Save) - gwarancja natychmiastowego odbioru
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    // 5. SZTYWNY KANAŁ 1 (Musi być dokładnie ten sam co w Access Point nadajnika!)
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    // 6. Inicjalizacja ESP-NOW i rejestracja callbacku odbiorczego
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    // 7. Wypisanie własnego adresu MAC w konsoli
    uint8_t my_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, my_mac);
    ESP_LOGW(TAG, "--------------------------------------------------");
    ESP_LOGW(TAG, "Odbiornik uruchomiony na kanale 1!");
    ESP_LOGW(TAG, "Moj adres MAC: " MACSTR, MAC2STR(my_mac));
    ESP_LOGW(TAG, "--------------------------------------------------");
}

/*
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

// ========================================================
// 1. TUTAJ ZMIENIASZ WARTOŚCI DO TESTÓW (Zakres: 0 - 255)
// ========================================================
#define TEST_R   255    // Czerwony
#define TEST_G   208  // Zielony
#define TEST_B   145    // Niebieski
#define TEST_W   0  // Dioda biała (np. 100 na test czystej bieli)

// ========================================================
// KONFIGURACJA SPRZĘTOWA
// ========================================================
#define LED_STRIP_GPIO       4
#define HORIZONTAL_DIODES    20
#define VERTICAL_DIODES      13
#define LED_COUNT (VERTICAL_DIODES * 2 + HORIZONTAL_DIODES * 2) // 66 diod

static const char *TAG = "LED_TEST";
static led_strip_handle_t led_strip;

static void init_leds(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_COUNT,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRBW, // SK6812 4-kanałowe
        .led_model = LED_MODEL_SK6812,
        .flags = {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "Pasek SK6812 RGBW zainicjalizowany na GPIO %d (%d diod)", LED_STRIP_GPIO, LED_COUNT);
}

void app_main(void)
{
    init_leds();

    // Pierwsze 33 diody: TYLKO FIZYCZNY CHIP BIAŁY
    for (int i = 0; i < 33; i++) {
        led_strip_set_pixel_rgbw(led_strip, i, 0, 0, 0, 250);
    }

    // Kolejne 33 diody: MIESZANKA RGB (nasza próba podrobienia chipu W)
    for (int i = 33; i < 66; i++) {
            led_strip_set_pixel_rgbw(led_strip, i, 255, 160, 140, 0); // Tu zmieniasz wartości
    }

    led_strip_refresh(led_strip);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}*/