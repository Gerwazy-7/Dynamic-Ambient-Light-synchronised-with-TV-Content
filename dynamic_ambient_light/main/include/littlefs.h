#ifndef LITTLEFS_H
#define LITTLEFS_H

/**
 * @brief Initialize NVS flash and mount the LittleFS virtual filesystem.
 *
 * Initializes non-volatile storage (required by Wi-Fi), configures the 
 * VFS driver for the partition (named "storage"), and mounts it 
 * in the directory path named "/web"
 */
void init_littleFS(void);

#endif // LITTLEFS_H