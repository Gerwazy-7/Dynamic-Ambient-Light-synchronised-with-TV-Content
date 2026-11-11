#ifndef WIFI_H
#define WIFI_H

/**
 * @brief Initialize and start the Wi-Fi interface in SoftAP mode.
 *
 * Configures the TCP/IP stack, event loop, SoftAP network interface,
 * Wi-Fi driver credentials, and starts the Access Point.
 */
void init_wifi_ap_mode(void);

#endif //WIFI_H