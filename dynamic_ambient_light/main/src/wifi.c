#include "wifi.h"

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI("WIFI AP", "New device connected. It's MAC: " MACSTR ", AID : %d", MAC2STR(event->mac), event ->aid);
    } 
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI("WIFI AP", "Device disconnected. It's MAC: " MACSTR ", AID : %d", MAC2STR(event->mac), event ->aid);
    }
}

void init_wifi_ap_mode(void)
{
    // Inicjalizacja stosu TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Utworzenie domyślnej pętli zdarzeń systemowych
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Utworzenie domyślnego interfejsu sieciowego dla AP
    esp_netif_create_default_wifi_ap();

    //Inicjalizacja konfiguracji sterownika Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    // Konfiguracja parametrów sieci Wi-Fi
    wifi_config_t wifi_config = 
    {
        .ap = 
        {
            .ssid = "ESP32-S3",
            .ssid_len = 0,
            .channel = 1,
            .password = "12345678",
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI AP", "WI-FI AP started...");
    ESP_LOGI("WIFI AP", "SSID: %s, Password: %s", (char *)wifi_config.ap.ssid, (char *)wifi_config.ap.password);
}
