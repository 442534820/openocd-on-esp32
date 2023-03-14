#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_vfs_fat.h"

#include "storage.h"

#define STORAGE_NAMESPACE           "NVS_DATA"


static const char *TAG = "storage";

esp_err_t storage_init_filesystem(void)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formatted before
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_fat_spiflash_mount_ro("/data", NULL, &mount_config);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find FATFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize FATFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_vfs_fat_info("/data", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

esp_err_t storage_nvs_write(const char *key, const char *value, size_t size)
{
    nvs_handle_t my_handle;

    ESP_RETURN_ON_ERROR((!key || !value), TAG, "Key or Value is NULL");
    ESP_RETURN_ON_ERROR(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle), TAG, "Failed to open namespace");
    ESP_RETURN_ON_ERROR(nvs_set_blob(my_handle, key, value, size), TAG, "Failed to set blob \"%s\"", key);
    ESP_RETURN_ON_ERROR(nvs_commit(my_handle), TAG, "Failed save changes");
    nvs_close(my_handle);

    return ESP_OK;
}

esp_err_t storage_nvs_read(const char *key, char *value, size_t size)
{
    nvs_handle_t my_handle;
    size_t n_bytes = size;

    ESP_RETURN_ON_ERROR(!key, TAG, "Key is NULL");
    ESP_RETURN_ON_ERROR(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle), TAG, "Failed to open namespace");
    ESP_RETURN_ON_ERROR(nvs_get_blob(my_handle, key, value, &n_bytes), TAG, "Failed to get blob \"%s\"", key);
    nvs_close(my_handle);

    if (n_bytes != size) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

int storage_nvs_erase_key(const char *key)
{
    nvs_handle_t my_handle;

    ESP_RETURN_ON_ERROR(!key, TAG, "Key is NULL");
    ESP_RETURN_ON_ERROR(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle), TAG, "Failed to open namespace");
    ESP_RETURN_ON_ERROR(nvs_erase_key(my_handle, key), TAG, "Failed to erase \"%s\"", key);
    nvs_close(my_handle);

    return ESP_OK;
}

size_t storage_nvs_get_value_length(const char *key)
{
    nvs_handle_t my_handle;
    size_t length;

    if (!key) {
        ESP_LOGE(TAG, "Key is NULL");
        return 0;
    }

    esp_err_t ret = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open namespace");
        return 0;
    }

    ret = nvs_get_blob(my_handle, key, NULL, &length);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get blob \"%s\"", key);
        return 0;
    }

    nvs_close(my_handle);

    return length;
}

bool storage_nvs_is_key_exist(const char *key)
{
    return storage_nvs_get_value_length(key) > 0;
}

esp_err_t storage_nvs_erase_everything(void)
{
    return nvs_flash_erase();
}

static bool will_be_filtered(const char *file_name)
{
    const char *filtered_files[] = {"esp_common.cfg", "xtensa-core-esp32"};

    for (int i = 0; i < sizeof(filtered_files) / sizeof(char *); i++) {
        if (strstr(file_name, filtered_files[i]) != NULL) {
            return true;
        }
    }
    return false;
}

char *storage_get_config_files(void)
{
    DIR *d;
    struct dirent *dir;
    char *str = NULL;

    d = opendir("/data/target");
    if (!d) {
        ESP_LOGE(TAG, "Could not open the directory");
        return NULL;
    }

    while ((dir = readdir(d)) != NULL) {
        if (str == NULL) {
            str = (char *) calloc(sizeof(dir->d_name), sizeof(char));
            if (!str) {
                ESP_LOGE(TAG, "Allocation error during fetch config file names");
                return NULL;
            }
            strcpy(str, dir->d_name);
        } else {
            if (!will_be_filtered(dir->d_name)) {
                int current_length = strlen(dir->d_name) + strlen(str) + 1 + 1;
                if (current_length > sizeof(dir->d_name)) {
                    char *tmp_str = (char *) calloc(current_length, sizeof(char));
                    if (!tmp_str) {
                        ESP_LOGE(TAG, "Allocation error");
                        free(str);
                        closedir(d);
                        return NULL;
                    }
                    strcpy(tmp_str, str);
                    free(str);
                    str = tmp_str;
                }
                str = strcat(str, "\n");
                str = strcat(str, dir->d_name);
            }
        }
    }
    closedir(d);

    return str;
}

esp_err_t storage_nvs_read_param(char *key, char **value)
{
    if (!key || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    /* First read length of the config string */
    size_t len = storage_nvs_get_value_length(key);
    if (len == 0) {
        return ESP_FAIL;
    }

    /* allocate memory for the null terminated string */
    char *ptr = (char *)calloc(len + 1, sizeof(char));
    if (!ptr) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return ESP_FAIL;
    }

    esp_err_t ret_nvs = storage_nvs_read(key, ptr, len);
    if (ret_nvs != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get %s command", key);
        free(ptr);
        return ESP_FAIL;
    }

    *value = ptr;

    return ESP_OK;
}
