#include <assert.h>
#include <string.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>

#include "storage.h"
#include "wifi.h"

#define MAX_CONNECTION_RETRY_COUNT 3
#define CONNECTION_TIMEOUT_MS      15000

#define WIFI_SAVED_NETWORKS_STORAGE_KEY "wifi_networks"

// Assumes dst is a statically allocated array
#define TRUNCATED_STRING_COPY(dst, src) \
    strncpy(dst, src, sizeof(dst)); \
    dst[sizeof(dst) - 1] = '\0';

typedef struct {
    wifi_network_credentials networks[WIFI_MAX_SAVED_NETWORKS];
    int count;
} wifi_saved_network_info;

ESP_EVENT_DEFINE_BASE(NETWORK_EVENT);

static SemaphoreHandle_t s_wifi_lock;
static SemaphoreHandle_t s_wifi_storage_lock;
static EventGroupHandle_t s_wifi_event_group;  // For blocking on connect
static wifi_saved_network_info s_saved_networks = {0};
static esp_netif_t* s_wifi_iface = NULL;
static volatile bool s_is_connected = false;

static void _set_connection_status(bool connected)
{
    s_is_connected = connected;
}

static void _on_disconnect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
    ESP_LOGI(__func__, "Disconnected from network %s", event->ssid);

    _set_connection_status(false);

    network_event network_event_id = (event->reason == WIFI_REASON_ASSOC_LEAVE) ?
        NETWORK_EVENT_LEFT :
        NETWORK_EVENT_DROPPED;

    xEventGroupSetBits(s_wifi_event_group, network_event_id);

    ESP_ERROR_CHECK(esp_event_post(
        NETWORK_EVENT,
        network_event_id,
        NULL,
        0,
        portMAX_DELAY
    ));
}

static void _on_connect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;

    char address[16] = {0};
    esp_ip4addr_ntoa(&event->ip_info.ip, address, sizeof(address));
    ESP_LOGI(__func__, "Got IP: %s", address);

    wifi_ap_record_t ap_info = { 0 };

    // If we are actually disconnected, we will get a disconnect event soon
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        network_event_connected connect_event = {0};
        TRUNCATED_STRING_COPY(connect_event.ssid, (char*)ap_info.ssid);

        _set_connection_status(true);
        xEventGroupSetBits(s_wifi_event_group, NETWORK_EVENT_CONNECTED);

        ESP_ERROR_CHECK(esp_event_post(
            NETWORK_EVENT,
            NETWORK_EVENT_CONNECTED,
            &connect_event,
            sizeof(connect_event),
            portMAX_DELAY
        ));
    }
}

void wifi_initialize()
{
    s_wifi_lock = xSemaphoreCreateMutex();
    s_wifi_storage_lock = xSemaphoreCreateMutex();

    s_wifi_event_group = xEventGroupCreate();

    // Load previously saved credentials
    void* saved_networks = storage_get_blob(WIFI_SAVED_NETWORKS_STORAGE_KEY);
    if (saved_networks)
    {
        memcpy(&s_saved_networks, saved_networks, sizeof(s_saved_networks));
        free(saved_networks);
    }

    _set_connection_status(false);

    // Init TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_iface = esp_netif_create_default_wifi_sta();

    // Station mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &_on_disconnect, NULL, NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &_on_connect, NULL, NULL
    ));

    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_deinitialize()
{
    wifi_disconnect();

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());

    esp_netif_destroy_default_wifi(s_wifi_iface);
    ESP_ERROR_CHECK(esp_netif_deinit());

    vEventGroupDelete(s_wifi_event_group);

    vSemaphoreDelete(s_wifi_storage_lock);
    vSemaphoreDelete(s_wifi_lock);
}

void wifi_scan(wifi_ap_info* out_ap_list, uint16_t* ap_count)
{
    assert(xSemaphoreTake(s_wifi_lock, portMAX_DELAY) == pdTRUE);

    if (esp_wifi_scan_start(NULL, true) != ESP_OK)
    {
        // Could fail due to timeout or wifi still connecting
        *ap_count = 0;
        xSemaphoreGive(s_wifi_lock);
        return;
    }

    wifi_ap_record_t* temp_ap_list = calloc(*ap_count, sizeof(wifi_ap_record_t));

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(ap_count, temp_ap_list));

    // Convert result from internal format to public-facing one
    uint16_t total_aps_returned = 0;
    for (int i = 0; i < *ap_count; ++i)
    {
        wifi_ap_record_t* src = &temp_ap_list[i];
        wifi_ap_info* dst = &out_ap_list[i];

        if (src->rssi < WIFI_MINIMUM_RSSI)
        {
            continue;
        }

        TRUNCATED_STRING_COPY(dst->ssid, (char*)src->ssid);

        dst->rssi = src->rssi;
        dst->channel = src->primary;
        dst->requires_password = src->authmode != WIFI_AUTH_OPEN;

        ++total_aps_returned;
    }

    free(temp_ap_list);
    *ap_count = total_aps_returned;

    xSemaphoreGive(s_wifi_lock);
}

static void _disconnect()
{
    xEventGroupClearBits(s_wifi_event_group, 0xFF);

    if (wifi_is_connected())
    {
        esp_wifi_disconnect();

        // Wait for disconnect
        xEventGroupWaitBits(
            s_wifi_event_group,
            NETWORK_EVENT_DROPPED | NETWORK_EVENT_LEFT,
            pdTRUE,  // xClearOnExit
            pdFALSE, // xWaitForAllBits
            portMAX_DELAY
        );
    }
}

bool wifi_connect(const char* ssid, const char* password, bool force)
{
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

    {
        assert(xSemaphoreTake(s_wifi_lock, portMAX_DELAY) == pdTRUE);

        bool did_connect = false;

        if (!force && wifi_is_connected())
        {
            // Connected to something else. Give up.
            // When the user chooses to connect, force = true.
            // When the connection manager tries to connect, force = false.
            ESP_LOGI(
                __func__,
                "Already connected to a network and new connection is not forced"
            );
        }
        else
        {
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));

            for (int i = 0; !did_connect && i < MAX_CONNECTION_RETRY_COUNT; ++i)
            {
                _disconnect();

                ESP_LOGI(
                    __func__,
                    "Trying to connect to network '%s' (attempt %d of %d)...",
                    ssid, i + 1, MAX_CONNECTION_RETRY_COUNT
                );

                xEventGroupClearBits(s_wifi_event_group, 0xFF);

                if (esp_wifi_connect() != ESP_OK)
                {
                    ESP_LOGE(__func__, "Connection failed");
                    break;
                }

                did_connect = (xEventGroupWaitBits(
                    s_wifi_event_group,
                    0xFF,     // Bits to wait for (any bits)
                    pdTRUE,   // xClearOnExit
                    pdFALSE,  // xWaitForAllBits
                    CONNECTION_TIMEOUT_MS / portTICK_PERIOD_MS
                ) & NETWORK_EVENT_CONNECTED) == NETWORK_EVENT_CONNECTED;
            }
        }

        xSemaphoreGive(s_wifi_lock);
        return did_connect;
    }
}

void wifi_disconnect()
{
    assert(xSemaphoreTake(s_wifi_lock, portMAX_DELAY) == pdTRUE);

    _disconnect();

    xSemaphoreGive(s_wifi_lock);
}

bool wifi_is_connected()
{
    //wifi_ap_record_t ap_info = { 0 };
    //return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
    return s_is_connected;
}


static void _wifi_flush_saved_networks()
{
    storage_set_blob(
        WIFI_SAVED_NETWORKS_STORAGE_KEY,
        &s_saved_networks,
        sizeof(s_saved_networks)
    );
}

static wifi_network_credentials* _get_saved_network(const char* ssid)
{
    for (int i = 0; i < s_saved_networks.count; ++i)
    {
        wifi_network_credentials* ap = &s_saved_networks.networks[i];
        if (strcmp(ap->ssid, ssid) == 0)
        {
            return ap;
        }
    }

    return NULL;
}

bool wifi_get_saved_network(const char* ssid, wifi_network_credentials* out_network)
{
    bool found = false;

    {
        assert(xSemaphoreTake(s_wifi_storage_lock, portMAX_DELAY) == pdTRUE);

        wifi_network_credentials* existing = _get_saved_network(ssid);
        if (existing != NULL)
        {
            *out_network = *existing;
            found = true;
        }

        xSemaphoreGive(s_wifi_storage_lock);
    }

    return found;
}

int wifi_get_all_saved_networks(wifi_network_credentials* out_networks)
{
    int count = 0;

    {
        assert(xSemaphoreTake(s_wifi_storage_lock, portMAX_DELAY) == pdTRUE);

        count = s_saved_networks.count;

        for (int i = 0; i < count; ++i)
        {
            out_networks[i] = s_saved_networks.networks[i];
        }

        xSemaphoreGive(s_wifi_storage_lock);
    }

    return count;
}

bool wifi_save_network(const char* ssid, const char* password)
{
    bool saved = false;

    {
        assert(xSemaphoreTake(s_wifi_storage_lock, portMAX_DELAY) == pdTRUE);

        wifi_network_credentials* existing = _get_saved_network(ssid);
        if (existing)
        {
            TRUNCATED_STRING_COPY(existing->pass, password);

            _wifi_flush_saved_networks();

            saved = true;
        }
        else if (s_saved_networks.count < WIFI_MAX_SAVED_NETWORKS)
        {
            wifi_network_credentials* ap = &s_saved_networks.networks[s_saved_networks.count];

            TRUNCATED_STRING_COPY(ap->ssid, ssid);
            TRUNCATED_STRING_COPY(ap->pass, password);

            ++s_saved_networks.count;
            _wifi_flush_saved_networks();

            saved = true;
        }
        else
        {
            // No room. Should have checked and cleared a spot first.
        }

        xSemaphoreGive(s_wifi_storage_lock);
    }

    return saved;
}

void wifi_forget_network(const char* ssid)
{
    assert(xSemaphoreTake(s_wifi_storage_lock, portMAX_DELAY) == pdTRUE);

    bool found_existing = false;

    for (int i = 0; i < s_saved_networks.count; ++i)
    {
        wifi_network_credentials* ap = &s_saved_networks.networks[i];

        if (found_existing)
        {
            // Shift left
            wifi_network_credentials* prev = &s_saved_networks.networks[i - 1];
            memcpy(prev, ap, sizeof(*prev));
            memset(ap, 0, sizeof(*ap));
        }
        else if (strcmp(ap->ssid, ssid) == 0)
        {
            found_existing = true;
            memset(ap, 0, sizeof(*ap));
        }
    }

    if (found_existing)
    {
        --s_saved_networks.count;
        _wifi_flush_saved_networks();
    }

    xSemaphoreGive(s_wifi_storage_lock);
}
