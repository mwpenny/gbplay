#include <esp_http_client.h>
#include <esp_log.h>

#include "http.h"

int http_get(const char* url, char* out, int out_len)
{
    esp_http_client_config_t config = {
        .url = url
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);

    esp_err_t err = esp_http_client_open(client, 0 /* write_len */);
    if (err != ESP_OK)
    {
        ESP_LOGE(__func__, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return -1;
    }

    int content_len = esp_http_client_fetch_headers(client);
    if (content_len < 0)
    {
        ESP_LOGE(__func__, "Failed to fetch HTTP headers");
        return -1;
    }

    // Truncate to fit in buffer
    int data_read = esp_http_client_read_response(client, out, out_len);
    if (data_read < 0)
    {
        ESP_LOGE(__func__, "Failed to read HTTP response");
        return -1;
    }

    // Success
    ESP_LOGI(__func__, "HTTP GET %s Status = %d, content_length = %d",
        url,
        esp_http_client_get_status_code(client),
        content_len
    );

    ESP_ERROR_CHECK(esp_http_client_cleanup(client));

    return data_read;
}
