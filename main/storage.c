#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "storage.h"
#include "esp_check.h"

static const char *TAG = "storage";
/* ssid:32 password:64 delimiter:1 null:1 */
size_t credentials_length = 98;

int mount_storage(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/data",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = false
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

int write_nvs(char *key, char *value, int size)
{
    nvs_handle_t my_handle;

    ESP_RETURN_ON_ERROR((!key || !value), TAG, "Key or Value is NULL");
    ESP_RETURN_ON_ERROR(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle), TAG, "Failed to open namespace");
    ESP_RETURN_ON_ERROR(nvs_set_blob(my_handle, key, value, size), TAG, "Failed to set blob \"%s\"", key);
    ESP_RETURN_ON_ERROR(nvs_commit(my_handle), TAG, "Failed save changes");

    nvs_close(my_handle);
    return ESP_OK;
}

int read_nvs(char *key, char *value, size_t *size)
{
    nvs_handle_t my_handle;

    ESP_RETURN_ON_ERROR(!key, TAG, "Key is NULL");
    ESP_RETURN_ON_ERROR(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle), TAG, "Failed to open namespace");
    ESP_RETURN_ON_ERROR(nvs_get_blob(my_handle, key, value, size), TAG, "Failed to get blob \"%s\"", key);

    nvs_close(my_handle);
    return ESP_OK;
}

int erase_nvs(void)
{
    return nvs_flash_erase();
}

int erase_key_nvs(char *key)
{
    nvs_handle_t my_handle;

    ESP_RETURN_ON_ERROR(!key, TAG, "Key is NULL");
    ESP_RETURN_ON_ERROR(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle), TAG, "Failed to open namespace");

    ESP_RETURN_ON_ERROR(nvs_erase_key(my_handle, key), TAG, "Failed to erase \"%s\"", key);
    nvs_close(my_handle);
    return ESP_OK;
}