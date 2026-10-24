#ifndef LITTLEFS_H
#define LITTLEFS_H

#include "esp_littlefs.h"
#include "esp_log.h"
#include "nvs_flash.h"

/**
 * Function defining configuration and registering virtual file system LittleFS
 */
void init_littleFS(void);

#endif // LITTLEFS_H