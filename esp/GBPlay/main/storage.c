#include <esp_log.h>
#include <nvs_flash.h>

#include "storage.h"

static nvs_handle storage_handle;

void storage_initialize()
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &storage_handle));
}

void storage_deinitialize()
{
    nvs_close(storage_handle);
}

char* storage_get_string(const char* key)
{
    size_t size = 0;
    if (nvs_get_str(storage_handle, key, NULL, &size) != ESP_OK)
    {
        // Does not exist
        return NULL;
    }

    char* str = malloc(size);
    ESP_ERROR_CHECK(nvs_get_str(storage_handle, key, str, &size));
    return str;
}

void storage_set_string(const char* key, const char* value)
{
    ESP_ERROR_CHECK(nvs_set_str(storage_handle, key, value));
    ESP_ERROR_CHECK(nvs_commit(storage_handle));

    ESP_LOGI(__func__, "Wrote %s to storage", key);
}
