#ifndef _WIFI_H
#define _WIFI_H

#include <stdint.h>

// Per 802.11 spec
#define WIFI_MAX_SSID_LENGTH        32
#define WIFI_MAX_PASS_LENGTH        64

#define WIFI_MINIMUM_RSSI          -80
#define WIFI_WEAK_RSSI_THRESHOLD   -70
#define WIFI_MED_RSSI_THRESHOLD    -60
#define WIFI_STRONG_RSSI_THRESHOLD -50

#define WIFI_MAX_SAVED_NETWORKS     5

typedef struct {
    char ssid[WIFI_MAX_SSID_LENGTH + 1];
    int8_t channel;
    int8_t rssi;
    bool requires_password;
} wifi_ap_info;

typedef struct {
    char ssid[WIFI_MAX_SSID_LENGTH + 1];
    char pass[WIFI_MAX_PASS_LENGTH + 1];
} wifi_network_credentials;

void wifi_initialize();
void wifi_deinitialize();

void wifi_scan(wifi_ap_info* ap_list, uint16_t* ap_count);
bool wifi_connect(const char* ssid, const char* password, bool force);
void wifi_disconnect();
bool wifi_is_connected();

bool wifi_get_saved_network(const char* ssid, wifi_network_credentials* out_network);
int wifi_get_all_saved_networks(wifi_network_credentials* out_networks);
bool wifi_save_network(const char* ssid, const char* password);
void wifi_forget_network(const char* ssid);

#endif
