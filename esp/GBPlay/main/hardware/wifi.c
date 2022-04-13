#include <string.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>

#include "wifi.h"

#define MAX_CONNECTION_RETRY_COUNT 3

typedef enum {
    CONNECTION_FAIL    = 1,
    CONNECTION_SUCCESS = 2
} WifiConnectionStatus;

static volatile bool is_connected = false;

static esp_netif_t* default_iface = NULL;

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t wifi_event_group;
static int connection_retry_count = 0;

static void _set_connection_status(bool connected)
{
    is_connected = connected;
}

static void _on_wifi_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    switch (event_id)
    {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(__func__, "Wi-Fi started");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(__func__, "Wi-Fi connected");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;

            _set_connection_status(false);

            if (event->reason == WIFI_REASON_ASSOC_LEAVE)
            {
                // Left voluntarily
                ESP_LOGI(__func__, "Left Wi-Fi network %s", event->ssid);
            }
            else if (connection_retry_count <= MAX_CONNECTION_RETRY_COUNT)
            {
                // Failed to connect, or AP went away
                ESP_LOGI(__func__, "Wi-Fi disconnected. Retrying connection to %s", event->ssid);
                ++connection_retry_count;
                esp_wifi_connect();
            }
            else
            {
                // Failed after retrying
                ESP_LOGI(__func__, "Failed to connect to Wi-Fi network %s", event->ssid);
                connection_retry_count = 0;
                xEventGroupSetBits(wifi_event_group, CONNECTION_FAIL);
            }
            break;
        }
        default:
            break;
    }
}

static void _on_ip_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    switch (event_id)
    {
        case IP_EVENT_STA_GOT_IP:
        {
            // Got IP, or it changed. Reconnect.
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;

            if (event->ip_changed)
            {
                // TODO: reconnect?
            }

            char address[16] = {0};
            esp_ip4addr_ntoa(&event->ip_info.ip, address, sizeof(address));
            ESP_LOGI(__func__, "Got IP: %s", address);

            _set_connection_status(true);
            connection_retry_count = 0;
            xEventGroupSetBits(wifi_event_group, CONNECTION_SUCCESS);
            break;
        }
        case IP_EVENT_STA_LOST_IP:
            // TODO: reconnect?
            break;
        default:
            break;
    }
}

void wifi_initialize()
{
    wifi_event_group = xEventGroupCreate();

    _set_connection_status(false);

    // Init TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    default_iface = esp_netif_create_default_wifi_sta();

    // Station mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &_on_wifi_event, NULL, NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, ESP_EVENT_ANY_ID, &_on_ip_event, NULL, NULL
    ));

    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_deinitialize()
{
    wifi_disconnect();

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());

    esp_netif_destroy_default_wifi(default_iface);
    ESP_ERROR_CHECK(esp_netif_deinit());

    vEventGroupDelete(wifi_event_group);
}

void wifi_scan(wifi_ap_info* ap_list, uint16_t* ap_count)
{
    if (esp_wifi_scan_start(NULL, true) != ESP_OK)
    {
        // Could fail due to timeout or wifi still connecting
        *ap_count = 0;
        return;
    }

    wifi_ap_record_t* temp_ap_list = calloc(*ap_count, sizeof(wifi_ap_record_t));

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(ap_count, temp_ap_list));

    // Convert result from internal format to public-facing one
    uint16_t total_aps_returned = 0;
    for (int i = 0; i < *ap_count; ++i)
    {
        wifi_ap_record_t* src = &temp_ap_list[i];
        wifi_ap_info* dst = &ap_list[i];

        if (src->rssi < WIFI_MINIMUM_RSSI)
        {
            continue;
        }

        strncpy(dst->ssid, (char*)src->ssid, WIFI_MAX_SSID_LENGTH + 1);
        dst->ssid[WIFI_MAX_SSID_LENGTH] = 0;

        dst->rssi = src->rssi;
        dst->channel = src->primary;
        dst->requires_password = src->authmode != WIFI_AUTH_OPEN;

        ++total_aps_returned;
    }

    free(temp_ap_list);
    *ap_count = total_aps_returned;
}

bool wifi_connect(const char* ssid, const char* password)
{
    wifi_disconnect();

    wifi_config_t cfg = { 0 };

    int max_ssid_len = sizeof(cfg.sta.ssid);
    int max_pass_len = sizeof(cfg.sta.password);
    int ssid_len = snprintf((char*)&cfg.sta.ssid, max_ssid_len, "%s", ssid);
    int pass_len = snprintf((char*)&cfg.sta.password, max_pass_len, "%s", password);

    if (ssid_len > max_ssid_len || pass_len > max_pass_len)
    {
        // This should never happen since SSIDs come from the hardware
        // and passwords are validated at the client layer
        ESP_LOGE(
            __func__,
            "Wi-Fi configuration out of bounds. SSID length: %d, password length: %d",
            ssid_len, pass_len
        );
        return false;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));

    if (esp_wifi_connect() != ESP_OK)
    {
        return false;
    }

    // Wait for connection or timeout
    return xEventGroupWaitBits(
        wifi_event_group,
        CONNECTION_FAIL | CONNECTION_SUCCESS,
        pdTRUE,  // xClearOnExit
        pdFALSE, // xWaitForAllBits
        portMAX_DELAY
    ) & CONNECTION_SUCCESS;
}

void wifi_disconnect()
{
    if (wifi_is_connected())
    {
        esp_wifi_disconnect();
    }
}

bool wifi_is_connected()
{
    //wifi_ap_record_t ap_info = { 0 };
    //return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
    return is_connected;
}
