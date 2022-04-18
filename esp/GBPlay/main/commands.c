#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <esp_log.h>

#include "http.h"
#include "hardware/wifi.h"

#define DEFAULT_SCAN_LIST_SIZE 10

static struct {
    struct arg_lit* save;
    struct arg_str* ssid;
    struct arg_str* pass;
    struct arg_end* end;
} wifi_connect_args;

static struct {
    struct arg_str* url;
    struct arg_end* end;
} http_get_args;

static struct {
    struct arg_int* index;
    struct arg_end* end;
} load_connection_args;

static struct {
    struct arg_int* index;
    struct arg_end* end;
} forget_connection_args;

static int _wifi_scan(int argc, char** argv)
{
    uint16_t ap_count = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_info ap_info[DEFAULT_SCAN_LIST_SIZE] = {0};

    wifi_scan(ap_info, &ap_count);

    ESP_LOGI(__func__, "Total APs scanned = %u\n", ap_count);
    for (int i = 0; i < ap_count; ++i)
    {
        ESP_LOGI(__func__, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(__func__, "CHANNEL \t\t%d", ap_info[i].channel);
        ESP_LOGI(__func__, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(__func__, "SECURE \t\t%d\n", ap_info[i].requires_password);
    }

    return 0;
}

static int _wifi_connect(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void**)&wifi_connect_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_connect_args.end, argv[0]);
        return 1;
    }

    const char* ssid = wifi_connect_args.ssid->sval[0];
    const char* pass = wifi_connect_args.pass->sval[0];

    if (wifi_connect(ssid, pass, true /* force */))
    {
        if (wifi_connect_args.save->count > 0)
        {
            if (!wifi_save_network(ssid, pass))
            {
                ESP_LOGW(
                    __func__,
                    "Could not save network %s. Maximum number are already saved.",
                    ssid
                );
            }
        }
        return 0;
    }

    return 1;
}

static int _wifi_disconnect(int argc, char** argv)
{
    wifi_disconnect();
    return 0;
}

static int _wifi_status(int argc, char** argv)
{
    if (!wifi_is_connected())
    {
        ESP_LOGI(__func__, "No connection");
    }
    else
    {
        ESP_LOGI(__func__, "Connected");
    }

    return 0;
}

static int _http_get(int argc, char** argv)
{
    if (!wifi_is_connected())
    {
        ESP_LOGI(__func__, "Please first establish a connection");
        return 1;
    }

    int nerrors = arg_parse(argc, argv, (void**)&http_get_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, http_get_args.end, argv[0]);
        return 1;
    }

    const char* url = http_get_args.url->sval[0];
    char body[1024] = {0};

    int data_read = http_get(url, body, sizeof(body));
    if (data_read < 0)
    {
        ESP_LOGE(__func__, "HTTP GET failed for %s", url);
    }

    ESP_LOGI(__func__, "%.*s", data_read, body);

    return (data_read < 0) ? 1 : 0;
}

static void _list_saved_networks(wifi_network_credentials* saved_networks, int count)
{
    ESP_LOGI(__func__, "Total saved networks = %d", count);
    for (int i = 0; i < count; ++i)
    {
        ESP_LOGI(__func__, "%d: %s", i, saved_networks[i].ssid);
    }
}

static int _load_connection(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**)&load_connection_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, load_connection_args.end, argv[0]);
        return 1;
    }

    wifi_network_credentials saved_networks[WIFI_MAX_SAVED_NETWORKS] = {0};
    int saved_network_count = wifi_get_all_saved_networks(saved_networks);

    if (load_connection_args.index->count == 0)
    {
        _list_saved_networks(saved_networks, saved_network_count);
        return 0;
    }
    else
    {
        int index = load_connection_args.index->ival[0];
        if (index < 0 || index >= saved_network_count)
        {
            ESP_LOGE(__func__, "No saved network with index %d", index);
            return 1;
        }

        wifi_network_credentials* ap = &saved_networks[index];
        return wifi_connect(ap->ssid, ap->pass, true /* force */) ? 0 : 1;
    }
}

static int _forget_connection(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**)&forget_connection_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, forget_connection_args.end, argv[0]);
        return 1;
    }

    wifi_network_credentials saved_networks[WIFI_MAX_SAVED_NETWORKS] = {0};
    int saved_network_count = wifi_get_all_saved_networks(saved_networks);

    if (forget_connection_args.index->count == 0)
    {
        _list_saved_networks(saved_networks, saved_network_count);
        return 0;
    }
    else
    {
        int index = forget_connection_args.index->ival[0];
        if (index < 0 || index >= saved_network_count)
        {
            ESP_LOGE(__func__, "No saved network with index %d", index);
            return 1;
        }

        wifi_forget_network(saved_networks[index].ssid);
        return 0;
    }
}


static void _register_wifi_scan()
{
    const esp_console_cmd_t scan_def = {
        .command = "scan",
        .help = "Scan available WiFi networks",
        .hint = NULL,
        .func = &_wifi_scan
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&scan_def));
}

static void _register_wifi_connect()
{
    wifi_connect_args.save = arg_lit0("s", "save", "Whether to save the configuration");
    wifi_connect_args.ssid = arg_str1(NULL, NULL, "ssid", "SSID of Wi-Fi network");
    wifi_connect_args.pass = arg_str1(NULL, NULL, "password", "Password of Wi-Fi network");
    wifi_connect_args.end = arg_end(10 /* max error count */);

    const esp_console_cmd_t connect_def = {
        .command = "connect",
        .help = "Connect to Wi-Fi network using specified credentials",
        .hint = NULL,
        .func = &_wifi_connect,
        .argtable = &wifi_connect_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&connect_def));
}

static void _register_wifi_disconnect()
{
    const esp_console_cmd_t disconnect_def = {
        .command = "disconnect",
        .help = "Disconnect from Wi-Fi network",
        .hint = NULL,
        .func = &_wifi_disconnect
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&disconnect_def));
}

static void _register_wifi_status()
{
    const esp_console_cmd_t status_def = {
        .command = "status",
        .help = "Report Wi-Fi status",
        .hint = NULL,
        .func = &_wifi_status
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_def));
}

static void _register_http_get()
{
    http_get_args.url = arg_str1(NULL, NULL, "url", "URL of web page to GET");
    http_get_args.end = arg_end(10 /* max error count */);

    const esp_console_cmd_t get_def = {
        .command = "query",
        .help = "Query an HTTP address and returns the result (specify HTTPS)",
        .hint = NULL,
        .func = &_http_get,
        .argtable = &http_get_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&get_def));
}

void _register_load_connection()
{
    load_connection_args.index = arg_int0(
        NULL,
        NULL,
        "index", "Index of saved network. Omit to list all saved networks."
    );
    load_connection_args.end = arg_end(10 /* max error count */);

    const esp_console_cmd_t load_def = {
        .command = "load",
        .help = "Load saved network configuration",
        .hint = NULL,
        .func = &_load_connection,
        .argtable = &load_connection_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&load_def));
}

void _register_forget_connection()
{
    forget_connection_args.index = arg_int0(
        NULL,
        NULL,
        "index", "Index of saved network. Omit to list all saved networks."
    );
    forget_connection_args.end = arg_end(10 /* max error count */);

    const esp_console_cmd_t forget_def = {
        .command = "forget",
        .help = "Forget saved network configuration",
        .hint = NULL,
        .func = &_forget_connection,
        .argtable = &forget_connection_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&forget_def));
}

void cmds_register()
{
    _register_wifi_scan();
    _register_wifi_connect();
    _register_wifi_disconnect();
    _register_wifi_status();

    _register_http_get();

    _register_load_connection();
    _register_forget_connection();

    esp_console_register_help_command();
}
