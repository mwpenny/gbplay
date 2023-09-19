#include <esp_log.h>
#include <nvs_flash.h>

#include "storage.h"

#define NVS_NAMESPACE "storage"

static nvs_handle s_storage_handle;

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
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_storage_handle));
}

void storage_deinitialize()
{
    nvs_close(s_storage_handle);
}

void* storage_get_blob(const char* key)
{
    size_t size = 0;
    if (nvs_get_blob(s_storage_handle, key, NULL, &size) != ESP_OK)
    {
        // Does not exist
        return NULL;
    }

    void* blob = malloc(size);
    ESP_ERROR_CHECK(nvs_get_blob(s_storage_handle, key, blob, &size));
    return blob;
}

void storage_set_blob(const char* key, const void* value, size_t length)
{
    ESP_ERROR_CHECK(nvs_set_blob(s_storage_handle, key, value, length));
    ESP_ERROR_CHECK(nvs_commit(s_storage_handle));

    ESP_LOGI(__func__, "Wrote blob '%s' to storage", key);
}

char* storage_get_string(const char* key)
{
    size_t size = 0;
    if (nvs_get_str(s_storage_handle, key, NULL, &size) != ESP_OK)
    {
        // Does not exist
        return NULL;
    }

    char* str = malloc(size);
    ESP_ERROR_CHECK(nvs_get_str(s_storage_handle, key, str, &size));
    return str;
}

void storage_set_string(const char* key, const char* value)
{
    ESP_ERROR_CHECK(nvs_set_str(s_storage_handle, key, value));
    ESP_ERROR_CHECK(nvs_commit(s_storage_handle));

    ESP_LOGI(__func__, "Wrote string '%s' to storage", key);
}

void storage_delete(const char* key)
{
    esp_err_t err = nvs_erase_key(s_storage_handle, key);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(__func__, "Key '%s' does not exist in storage. Nothing to do.", key);
    }
    else
    {
        ESP_ERROR_CHECK(err);
        ESP_LOGI(__func__, "Deleted '%s' from storage", key);
    }

    ESP_ERROR_CHECK(nvs_commit(s_storage_handle));
}
