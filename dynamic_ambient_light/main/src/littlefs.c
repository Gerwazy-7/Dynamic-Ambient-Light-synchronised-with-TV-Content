#include "littlefs.h"

#include "esp_littlefs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"

void init_littleFS(void)
{
    // NVS initialization - needed for WiFi
    esp_err_t ret_NVS = nvs_flash_init();
    if (ret_NVS == ESP_ERR_NVS_NO_FREE_PAGES || ret_NVS == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret_NVS = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret_NVS);
    
    ESP_LOGI("LittleFS", "LittleFS initialization...");
        
    // Mount configuration
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/web", 
        .partition_label = "storage",     
        .format_if_mount_failed = true   
    };

    // VFS registering
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("LittleFS", "Couldn't mount file system.");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("LittleFS", "Couldn't find 'storage' partition");
        } else {
            ESP_LOGE("LittleFS", "Error while initializing LittleFS(%s)", esp_err_to_name(ret));
        }
        return;
    }
}