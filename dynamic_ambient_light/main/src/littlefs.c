#include "littlefs.h"

void init_littleFS(void)
{
    static const char *TAG = "LittleFS";
    ESP_LOGI(TAG, "Inicjalizacja LittleFS...");
        
    // 1. Definiowanie konfiguracji montowania
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/web",            // Ścieżka (folder) pod którą pliki będą widoczne w kodzie
        .partition_label = "storage",    // Nazwa partycji z Twojego pliku partitions.csv
        .format_if_mount_failed = true   // Automatycznie sformatuje pamięć przy pierwszym uruchomieniu
    };

    // 2. Rejestracja (montowanie) wirtualnego systemu plików (VFS)
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    // 3. Sprawdzenie, czy montowanie się powiodło
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Nie udało się zamontować ani sformatować systemu plików.");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Nie znaleziono partycji o nazwie 'storage' w tabeli partycji.");
        } else {
            ESP_LOGE(TAG, "Błąd podczas inicjalizacji LittleFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    // Inicjalizacja NVS w celu uruchomienia Wi-Fi
    esp_err_t ret_NVS = nvs_flash_init();
    if (ret_NVS == ESP_ERR_NVS_NO_FREE_PAGES || ret_NVS == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret_NVS = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret_NVS);
}