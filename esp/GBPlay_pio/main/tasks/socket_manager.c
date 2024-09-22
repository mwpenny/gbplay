#include <errno.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include <esp_event.h>
#include <esp_log.h>

#include "../hardware/spi.h"
#include "../hardware/storage.h"
#include "../hardware/wifi.h"
#include "socket.h"

#define TASK_NAME "socket-manager"

#define CONNECTION_TIMEOUT_MS 10000

#define SERVER_HOST_STORAGE_KEY "server_host"
#define SERVER_PORT_STORAGE_KEY "server_port"

#define DEFAULT_SERVER_HOST "192.168.10.170"
#define DEFAULT_SERVER_PORT 1989

static TaskHandle_t s_socket_manager_task;

static void _on_network_connect(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // Wake up the task
    xTaskNotify(s_socket_manager_task, 0, eNoAction);
}

static int _connect_to_server()
{
    bool should_free_host = true;

    char* server_host = storage_get_string(SERVER_HOST_STORAGE_KEY);
    if (server_host == NULL)
    {
        server_host = DEFAULT_SERVER_HOST;
        should_free_host = false;
    }

    uint16_t server_port = DEFAULT_SERVER_PORT;
    char* server_port_str = storage_get_string(SERVER_PORT_STORAGE_KEY);
    if (server_port_str != NULL)
    {
        long ret = strtol(server_port_str, NULL, 10 /* base */);
        if (ret == 0 || errno == ERANGE || ret > UINT16_MAX)
        {
            ESP_LOGW(
                TASK_NAME,
                "Configured server port %s is invalid. Using default port of %d.",
                server_port_str,
                server_port
            );
        }
        else
        {
            server_port = ret;
        }

        free(server_port_str);
    }

    ESP_LOGI(TASK_NAME, "Connecting to backend server at %s:%d", server_host, server_port);

    int sock = socket_connect(server_host, server_port, CONNECTION_TIMEOUT_MS);

    if (should_free_host)
    {
        free(server_host);
    }

    return sock;
}

static void _handle_data_until_error(int sock)
{
    // TODO: abstract this to use a generic client, rather than socket
    //  Will making it easier to handle different types of packets
    while (true)
    {
        uint8_t rx = 0;
        if (!socket_read(sock, &rx, sizeof(rx)))
        {
            break;
        }

        uint8_t tx = spi_exchange_byte(rx);
        if (!socket_write(sock, &tx, sizeof(tx)))
        {
            break;
        }
    }
}

static void task_socket_manager(void *data)
{
    while (true)
    {
        if (!wifi_is_connected())
        {
            xTaskNotifyWait(
                0,              // ulBitsToClearOnEntry
                0,              // ulBitsToClearOnExit
                NULL,           // pulNotificationValue
                portMAX_DELAY   // xTicksToWait
            );

            ESP_LOGI(TASK_NAME, "Network connection established. Opening socket...");
        }
        else
        {
            ESP_LOGI(TASK_NAME, "Retrying socket connection...");
        }

        int sock = _connect_to_server();
        if (sock < 0)
        {
            ESP_LOGE(TASK_NAME, "Failed to connect to backend server");
        }
        else
        {
            ESP_LOGI(TASK_NAME, "Successfully connected to backend server");

            _handle_data_until_error(sock);

            ESP_LOGI(TASK_NAME, "Closing socket");
            close(sock);
        }
    }
}

void task_socket_manager_start(int core, int priority)
{
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        NETWORK_EVENT, NETWORK_EVENT_CONNECTED, &_on_network_connect, NULL, NULL
    ));

    xTaskCreatePinnedToCore(
        &task_socket_manager,
        TASK_NAME,
        4096,                   // Stack size
        NULL,                   // Arguments
        priority,               // Priority
        &s_socket_manager_task, // Task handle (output parameter)
        core                    // CPU core ID
    );
}
