#include <string.h>
#include <unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "../hardware/wifi.h"

#define TASK_NAME "network-manager"

#define IDLE_SCAN_PERIOD_SECONDS    30
#define SCAN_LIST_SIZE              10
#define NETWORK_HISTORY_SIZE        WIFI_MAX_SAVED_NETWORKS
#define MINUTE_MICROSECONDS         (60 * 1000 * 1000)
#define NETWORK_BLOCK_MINUTES       5

typedef struct {
    char ssid[WIFI_MAX_SSID_LENGTH + 1];
    int64_t blocked_until;  // For blacklisting networks on disconnect
} network_info;

// Ring buffer of metadata for previously-used networks
typedef struct {
    network_info used_networks[NETWORK_HISTORY_SIZE];
    char prev_ssid[WIFI_MAX_SSID_LENGTH + 1];
    int next_index;
} network_history;

static EventGroupHandle_t s_connection_event_group;
static network_history s_network_history = {0};

static void _on_disconnect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    xEventGroupSetBits(s_connection_event_group, NETWORK_EVENT_DROPPED);
}

static void _on_leave(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    xEventGroupSetBits(s_connection_event_group, NETWORK_EVENT_LEFT);
}

static void _on_connect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    network_event_connected* event = (network_event_connected*)event_data;

    strncpy(s_network_history.prev_ssid, (char*)event->ssid, sizeof(s_network_history.prev_ssid));
    s_network_history.prev_ssid[sizeof(s_network_history.prev_ssid) - 1] = '\0';

    xEventGroupSetBits(s_connection_event_group, NETWORK_EVENT_CONNECTED);
}

static network_info* _ensure_network_info(const char* ssid)
{
    for (int i = 0; i < NETWORK_HISTORY_SIZE; ++i)
    {
        network_info* entry = &s_network_history.used_networks[i];
        if (strcmp(entry->ssid, ssid) == 0)
        {
            return entry;
        }
    }

    // Insert into ring buffer, overwriting oldest if necessary
    int index = s_network_history.next_index;
    s_network_history.next_index = \
        (s_network_history.next_index + 1) % NETWORK_HISTORY_SIZE;

    network_info* entry = &s_network_history.used_networks[index];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->ssid, ssid, WIFI_MAX_SSID_LENGTH);
    entry->ssid[WIFI_MAX_SSID_LENGTH] = '\0';
    return entry;
}

static void _clear_network_history()
{
    memset(&s_network_history.used_networks, 0, sizeof(s_network_history.used_networks));
    s_network_history.next_index = 0;
}

static void _block_network(network_info* info)
{
    ESP_LOGI(
        TASK_NAME,
        "Blocking network '%s' for %d minutes",
        info->ssid,
        NETWORK_BLOCK_MINUTES
    );
    info->blocked_until = esp_timer_get_time() + (NETWORK_BLOCK_MINUTES * MINUTE_MICROSECONDS);
}

static bool _try_connect(wifi_network_credentials* creds)
{
    network_info* info = _ensure_network_info(creds->ssid);
    if (info->blocked_until > esp_timer_get_time())
    {
        ESP_LOGD(TASK_NAME, "Skipped network '%s' due to temporary block", creds->ssid);
        return false;
    }

    ESP_LOGI(TASK_NAME, "Trying network '%s'...", creds->ssid);
    if (wifi_connect(creds->ssid, creds->pass, false /* force */))
    {
        ESP_LOGI(TASK_NAME, "Successfully connected to network '%s'", creds->ssid);
        return true;
    }

    ESP_LOGI(TASK_NAME, "Failed to connect to network '%s'", creds->ssid);
    _block_network(info);
    return false;
}

static bool _try_connect_prev()
{
    wifi_network_credentials creds = {0};
    if (!wifi_get_saved_network(s_network_history.prev_ssid, &creds))
    {
        // Can't connect if not saved
        return false;
    }

    return _try_connect(&creds);
}

static bool _try_autoconnect()
{
    ESP_LOGI(TASK_NAME, "Trying to auto-connect to a network...");

    // Results will be sorted by RSSI in descending order
    wifi_ap_info available_aps[SCAN_LIST_SIZE] = {0};
    uint16_t ap_count = SCAN_LIST_SIZE;
    wifi_scan(available_aps, &ap_count);

    ESP_LOGI(TASK_NAME, "Found %d networks", ap_count);

    for (int i = 0; i < ap_count; ++i)
    {
        wifi_ap_info* ap = &available_aps[i];

        wifi_network_credentials creds = {0};
        if (wifi_get_saved_network(ap->ssid, &creds) && _try_connect(&creds))
        {
            return true;
        }
    }

    return false;
}

static void task_network_manager(void* data)
{
    // Future enhancements, probably overkill:
    // * Detect and avoid networks with rapidly changing RSSIs
    // * Prioritize based on wifi technology (n > g > b)
    // * Detect internet connection (resolve DNS; dns_gethostbyname)
    //     Likely good enough to just check this on configuration and let our application layer handle it
    //     Technically gbplay will still work with no internet connection if hosted locally
    // * Prefer networks that previously had internet access over ones that didn't
    //     New member of network_info
    // * Exponential backoff (for blacklisting and time between scans)

    while (true)
    {
        EventBits_t bits = xEventGroupWaitBits(
            s_connection_event_group,
            0xFF,    // Bits to wait for (any bits)
            pdTRUE,  // xClearOnExit
            pdFALSE, // xWaitForAllBits
            portMAX_DELAY
        );

        if (bits & NETWORK_EVENT_DROPPED)
        {
            // Could have been a false alarm (disconnect + reconnect by us)
            if (!wifi_is_connected())
            {
                ESP_LOGI(TASK_NAME, "Connection dropped");

                // Try to reconnect
                while (!wifi_is_connected() && !_try_connect_prev() && !_try_autoconnect())
                {
                    sleep(IDLE_SCAN_PERIOD_SECONDS);
                }
            }
        }
        else if (bits & NETWORK_EVENT_LEFT)
        {
            ESP_LOGI(TASK_NAME, "Left network voluntarily. Not attempting to reconnect.");

            // Deprioritize network the user chose to leave
            network_info* info = _ensure_network_info(s_network_history.prev_ssid);
            _block_network(info);
        }
        else if (bits & NETWORK_EVENT_CONNECTED)
        {
            ESP_LOGI(TASK_NAME, "Connected to network '%s'", s_network_history.prev_ssid);

            _clear_network_history();
        }
    }
}

void task_network_manager_start(int core, int priority)
{
    s_connection_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        NETWORK_EVENT, NETWORK_EVENT_DROPPED, &_on_disconnect, NULL, NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        NETWORK_EVENT, NETWORK_EVENT_LEFT, &_on_leave, NULL, NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        NETWORK_EVENT, NETWORK_EVENT_CONNECTED, &_on_connect, NULL, NULL
    ));

    xTaskCreatePinnedToCore(
        &task_network_manager,
        TASK_NAME,
        4096,                      // Stack size
        NULL,                      // Arguments
        priority,                  // Priority
        NULL,                      // Task handle (output parameter)
        core                       // CPU core ID
    );

    // Wake the task up and start looking for networks
    xEventGroupSetBits(s_connection_event_group, NETWORK_EVENT_DROPPED);
}
