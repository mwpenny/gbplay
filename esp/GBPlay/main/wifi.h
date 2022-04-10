#ifndef _WIFI_H
#define _WIFI_H

#include <stdint.h>

// Per 802.11 spec
#define WIFI_MAX_SSID_LENGTH 32

// Defining our own type to keep this interface generic
// TODO: multiple authentication types
typedef struct {
    char ssid[WIFI_MAX_SSID_LENGTH + 1];
    int8_t channel;
    int8_t rssi;
} wifi_ap_info;

void wifi_initialize();
void wifi_deinitialize();

void wifi_scan(wifi_ap_info* ap_list, uint16_t* ap_count);
bool wifi_connect(const char* ssid, const char* password);
void wifi_disconnect();
bool wifi_is_connected();

#endif
