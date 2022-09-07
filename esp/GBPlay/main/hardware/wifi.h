#ifndef _WIFI_H
#define _WIFI_H

#include <stdint.h>
#include <esp_event_base.h>

// Per 802.11 spec
#define WIFI_MAX_SSID_LENGTH        32
#define WIFI_MAX_PASS_LENGTH        64

#define WIFI_MINIMUM_RSSI          -80
#define WIFI_WEAK_RSSI_THRESHOLD   -70
#define WIFI_MED_RSSI_THRESHOLD    -60
#define WIFI_STRONG_RSSI_THRESHOLD -50

#define WIFI_MAX_SAVED_NETWORKS     5

ESP_EVENT_DECLARE_BASE(NETWORK_EVENT);

typedef enum {
    NETWORK_EVENT_DROPPED   = 1,     // Network connection lost unexpectedly
    NETWORK_EVENT_LEFT      = 2,     // Network connection intentionally closed
    NETWORK_EVENT_CONNECTED = 4      // Network connection established
} network_event;

typedef struct {
    char ssid[WIFI_MAX_SSID_LENGTH + 1];
} network_event_connected;

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

/* Enables the Wi-Fi module and configures it for use. */
void wifi_initialize();

/* Disables the Wi-Fi module. */
void wifi_deinitialize();

/*
    Scans for available Wi-Fi networks.

    @param out_ap_list  [output] List of found Wi-Fi networks
    @param ap_count     As input, maximum number of networks to return.
                        As output, number of networks actually returned.
*/
void wifi_scan(wifi_ap_info* out_ap_list, uint16_t* ap_count);

/*
    Attempts to connect to a Wi-Fi network.

    @param ssid     SSID of network to connect to
    @param password Password of network to connect to
    @param force    Whether to connect even if already connected to a network

    @returns Whether or not the connection succeeded
*/
bool wifi_connect(const char* ssid, const char* password, bool force);

/* If connected, disconnects from the current Wi-Fi network */
void wifi_disconnect();

/*
    Checks whether connected to a network.

    @returns Whether the device is connected to a Wi-Fi network
*/
bool wifi_is_connected();

/*
    Retrieves the credentials of a saved Wi-Fi network from memory.

    @param ssid        The SSID of the network to retrieve
    @param out_network [output] The saved network credentials, if found

    @returns Whether or not saved credentials were found
*/
bool wifi_get_saved_network(const char* ssid, wifi_network_credentials* out_network);

/*
    Retrieves the credentials of all saved Wi-Fi networks.

    @param out_networks [output] List of saved credentials. Must be large enough
                                 to contain WIFI_MAX_SAVED_NETWORKS entries.

    @returns The number of saved credentials returned
*/
int wifi_get_all_saved_networks(wifi_network_credentials* out_networks);

/*
    Saves the credentials of a Wi-Fi network.

    @param ssid     SSID of network to save
    @param password Password of network to save

    @returns Whether the network was successfully saved.
             Can fail if WIFI_MAX_SAVED_NETWORKS credentials are already saved.
*/
bool wifi_save_network(const char* ssid, const char* password);

/*
    Removes saved the credentials for a Wi-Fi network, if present.

    @param ssid     SSID of network to forget
*/
void wifi_forget_network(const char* ssid);

#endif
