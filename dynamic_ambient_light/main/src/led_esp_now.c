#include "led_esp_now.h"

#include <string.h>
#include "esp_mac.h"
#include "led_logic.h"

/**
 * @brief ESP-NOW receiver MAC
 */
static uint8_t recv_mac[ESP_NOW_ETH_ALEN] = {0x34, 0x98, 0x7a, 0xb7, 0xd5, 0xd8};

/**
 * @brief Callback function used after every EPS-NOW transmission attempt
 * It logs every lost packet send via ESP-NOW
 * @param tx_info Poiter to transmission metadata from WiFi driver. It contains receiver MAC address
 * @param status Information about status of sent packet (ESP_NOW_SEND_SUCCESS or ESP_NOW_SEND_FAIL)
 */
static void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS)
    {
        if(tx_info != NULL)
        {
            ESP_LOGE("ESP-NOW TX", "Packet lost! Receiver MAC: ", MACSTR, MAC2STR(tx_info->des_addr));
        }
        else
        {
            ESP_LOGE("ESP_NOW TX", "Transmission error");
        }
    }
}


esp_err_t esp_now_initialization(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, recv_mac, ESP_NOW_ETH_ALEN);
    peer_info.channel = 1;
    peer_info.ifidx = WIFI_IF_AP;
    peer_info.encrypt = false;

    ESP_ERROR_CHECK (esp_now_add_peer(&peer_info));
    ESP_LOGI("ESP-NOW TX", "ESP-NOW zainicjalizowane");

    return ESP_OK;
}

esp_err_t send_led_colours(const output_rgb_struct *results)
{
    if (results == NULL)
    {
        ESP_LOGE("ESP-NOW TX", "Brak struktury z kolorami");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = esp_now_send(recv_mac, (const uint8_t *)results, sizeof(output_rgb_struct));

    if (err != ESP_OK)
    {
        ESP_LOGE("ESP-NOW TX", "Blad wysylania danych LED");
        return err;
    }
    return ESP_OK;
} 