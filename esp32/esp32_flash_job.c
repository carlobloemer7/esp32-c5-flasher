#include "esp32_flash_job.h"
#include "esp32_paths.h"

#include <stdlib.h>
#include <string.h>

static const char* const category_folders[Esp32FlashCategoryCount] = {
    [Esp32FlashCategoryBootloader] = ESP32_BOOTLOADER_FOLDER,
    [Esp32FlashCategoryPartitionTable] = ESP32_PARTITION_TABLE_FOLDER,
    [Esp32FlashCategoryOtaData] = ESP32_OTA_DATA_FOLDER,
    [Esp32FlashCategoryFirmware] = ESP32_FIRMWARE_FOLDER,
};

static const char* const category_names[Esp32FlashCategoryCount] = {
    [Esp32FlashCategoryBootloader] = "Bootloader",
    [Esp32FlashCategoryPartitionTable] = "Partitionstabelle",
    [Esp32FlashCategoryOtaData] = "OTA Data (optional)",
    [Esp32FlashCategoryFirmware] = "Firmware/App",
};

static const uint32_t category_offsets[Esp32FlashCategoryCount] = {
    [Esp32FlashCategoryBootloader] = ESP32_OFFSET_BOOTLOADER,
    [Esp32FlashCategoryPartitionTable] = ESP32_OFFSET_PARTITION_TABLE,
    [Esp32FlashCategoryOtaData] = ESP32_OFFSET_OTA_DATA,
    [Esp32FlashCategoryFirmware] = ESP32_OFFSET_FIRMWARE,
};

const char* esp32_flash_job_category_folder(Esp32FlashCategory category) {
    furi_assert(category < Esp32FlashCategoryCount);
    return category_folders[category];
}

const char* esp32_flash_job_category_name(Esp32FlashCategory category) {
    furi_assert(category < Esp32FlashCategoryCount);
    return category_names[category];
}

uint32_t esp32_flash_job_category_offset(Esp32FlashCategory category) {
    furi_assert(category < Esp32FlashCategoryCount);
    return category_offsets[category];
}

bool esp32_flash_job_category_optional(Esp32FlashCategory category) {
    return category == Esp32FlashCategoryOtaData;
}

Esp32FlashJob* esp32_flash_job_alloc(void) {
    Esp32FlashJob* job = malloc(sizeof(Esp32FlashJob));
    memset(job, 0, sizeof(Esp32FlashJob));
    for(size_t i = 0; i < Esp32FlashCategoryCount; i++) {
        job->categories[i].path = furi_string_alloc();
        job->categories[i].offset = category_offsets[i];
    }
    for(size_t i = 0; i < ESP32_CUSTOM_ENTRIES_MAX; i++) {
        job->custom[i].path = furi_string_alloc();
    }
    job->custom_count = 0;
    job->auto_reset_enabled = false;
    job->gpio27_assist_enabled = false;
    job->baud_rate = 115200;
    return job;
}

void esp32_flash_job_free(Esp32FlashJob* job) {
    furi_check(job);
    for(size_t i = 0; i < Esp32FlashCategoryCount; i++) {
        furi_string_free(job->categories[i].path);
    }
    for(size_t i = 0; i < ESP32_CUSTOM_ENTRIES_MAX; i++) {
        furi_string_free(job->custom[i].path);
    }
    free(job);
}

void esp32_flash_job_ensure_folders(Storage* storage) {
    storage_common_mkdir(storage, ESP32_APP_DATA_FOLDER);
    storage_common_mkdir(storage, ESP32_BOOTLOADER_FOLDER);
    storage_common_mkdir(storage, ESP32_PARTITION_TABLE_FOLDER);
    storage_common_mkdir(storage, ESP32_OTA_DATA_FOLDER);
    storage_common_mkdir(storage, ESP32_FIRMWARE_FOLDER);
    storage_common_mkdir(storage, ESP32_CUSTOM_FOLDER);
}

void esp32_flash_job_set_category_path(
    Esp32FlashJob* job,
    Esp32FlashCategory category,
    const char* path) {
    furi_assert(category < Esp32FlashCategoryCount);
    furi_string_set(job->categories[category].path, path);
    // Manual assignment always uses the standard ESP-IDF offset for this
    // category, overriding any preset-specific offset (e.g. GhostESP's
    // bootloader@0x0) left over from a previous preset selection.
    job->categories[category].offset = category_offsets[category];
}

void esp32_flash_job_set_category_entry(
    Esp32FlashJob* job,
    Esp32FlashCategory category,
    const char* path,
    uint32_t offset) {
    furi_assert(category < Esp32FlashCategoryCount);
    furi_string_set(job->categories[category].path, path);
    job->categories[category].offset = offset;
}

bool esp32_flash_job_add_custom(Esp32FlashJob* job, const char* path, uint32_t offset) {
    if(job->custom_count >= ESP32_CUSTOM_ENTRIES_MAX) return false;
    furi_string_set(job->custom[job->custom_count].path, path);
    job->custom[job->custom_count].offset = offset;
    job->custom_count++;
    return true;
}

void esp32_flash_job_remove_custom(Esp32FlashJob* job, size_t index) {
    if(index >= job->custom_count) return;
    for(size_t i = index; i + 1 < job->custom_count; i++) {
        furi_string_set(job->custom[i].path, furi_string_get_cstr(job->custom[i + 1].path));
        job->custom[i].offset = job->custom[i + 1].offset;
    }
    job->custom_count--;
    furi_string_reset(job->custom[job->custom_count].path);
}

size_t esp32_flash_job_resolved_count(const Esp32FlashJob* job) {
    size_t count = 0;
    for(size_t i = 0; i < Esp32FlashCategoryCount; i++) {
        if(!furi_string_empty(job->categories[i].path)) count++;
    }
    count += job->custom_count;
    return count;
}

bool esp32_flash_job_resolved_get(
    const Esp32FlashJob* job,
    size_t entry_index,
    FuriString* out_path,
    uint32_t* out_offset) {
    size_t idx = 0;
    for(size_t i = 0; i < Esp32FlashCategoryCount; i++) {
        if(furi_string_empty(job->categories[i].path)) continue;
        if(idx == entry_index) {
            furi_string_set(out_path, job->categories[i].path);
            *out_offset = job->categories[i].offset;
            return true;
        }
        idx++;
    }
    for(size_t i = 0; i < job->custom_count; i++) {
        if(idx == entry_index) {
            furi_string_set(out_path, job->custom[i].path);
            *out_offset = job->custom[i].offset;
            return true;
        }
        idx++;
    }
    return false;
}

bool esp32_flash_job_is_ready(const Esp32FlashJob* job) {
    return !furi_string_empty(job->categories[Esp32FlashCategoryBootloader].path) &&
           !furi_string_empty(job->categories[Esp32FlashCategoryPartitionTable].path) &&
           !furi_string_empty(job->categories[Esp32FlashCategoryFirmware].path);
}

// --- config.txt persistence -------------------------------------------------
// Simple line-based "key=value" format, one entry per line. Not a general
// purpose parser: the file is only ever written by this app.

#define CONFIG_MAX_SIZE 8192

static void parse_line(Esp32FlashJob* job, const char* line) {
    const char* eq = strchr(line, '=');
    if(!eq) return;
    size_t key_len = (size_t)(eq - line);
    const char* value = eq + 1;

    char key[32];
    if(key_len >= sizeof(key)) return;
    memcpy(key, line, key_len);
    key[key_len] = '\0';

    if(strcmp(key, "auto_reset") == 0) {
        job->auto_reset_enabled = atoi(value) != 0;
    } else if(strcmp(key, "gpio27_assist") == 0) {
        job->gpio27_assist_enabled = atoi(value) != 0;
    } else if(strcmp(key, "baud_rate") == 0) {
        job->baud_rate = (uint32_t)atoi(value);
    } else if(strcmp(key, "bootloader") == 0) {
        furi_string_set(job->categories[Esp32FlashCategoryBootloader].path, value);
    } else if(strcmp(key, "partition_table") == 0) {
        furi_string_set(job->categories[Esp32FlashCategoryPartitionTable].path, value);
    } else if(strcmp(key, "ota_data") == 0) {
        furi_string_set(job->categories[Esp32FlashCategoryOtaData].path, value);
    } else if(strcmp(key, "firmware") == 0) {
        furi_string_set(job->categories[Esp32FlashCategoryFirmware].path, value);
    } else if(strcmp(key, "custom_count") == 0) {
        int count = atoi(value);
        if(count < 0) count = 0;
        if(count > ESP32_CUSTOM_ENTRIES_MAX) count = ESP32_CUSTOM_ENTRIES_MAX;
        job->custom_count = (size_t)count;
    } else if(strncmp(key, "custom", 6) == 0) {
        // custom<N>_path / custom<N>_offset
        char* underscore = strchr(key + 6, '_');
        if(!underscore) return;
        int n = atoi(key + 6);
        if(n < 0 || n >= ESP32_CUSTOM_ENTRIES_MAX) return;
        if(strcmp(underscore, "_path") == 0) {
            furi_string_set(job->custom[n].path, value);
        } else if(strcmp(underscore, "_offset") == 0) {
            job->custom[n].offset = (uint32_t)strtoul(value, NULL, 0);
        }
    }
}

void esp32_flash_job_load(Esp32FlashJob* job, Storage* storage) {
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, ESP32_CONFIG_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return;
    }

    char* buffer = malloc(CONFIG_MAX_SIZE);
    size_t read = storage_file_read(file, buffer, CONFIG_MAX_SIZE - 1);
    storage_file_close(file);
    storage_file_free(file);
    buffer[read] = '\0';

    char* line = strtok(buffer, "\r\n");
    while(line) {
        parse_line(job, line);
        line = strtok(NULL, "\r\n");
    }

    free(buffer);
}

bool esp32_flash_job_save(Esp32FlashJob* job, Storage* storage) {
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, ESP32_CONFIG_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!ok) {
        storage_file_free(file);
        return false;
    }

    FuriString* out = furi_string_alloc();
    furi_string_cat_printf(out, "auto_reset=%d\n", job->auto_reset_enabled ? 1 : 0);
    furi_string_cat_printf(out, "gpio27_assist=%d\n", job->gpio27_assist_enabled ? 1 : 0);
    furi_string_cat_printf(out, "baud_rate=%lu\n", (unsigned long)job->baud_rate);
    furi_string_cat_printf(
        out, "bootloader=%s\n", furi_string_get_cstr(job->categories[Esp32FlashCategoryBootloader].path));
    furi_string_cat_printf(
        out,
        "partition_table=%s\n",
        furi_string_get_cstr(job->categories[Esp32FlashCategoryPartitionTable].path));
    furi_string_cat_printf(
        out, "ota_data=%s\n", furi_string_get_cstr(job->categories[Esp32FlashCategoryOtaData].path));
    furi_string_cat_printf(
        out, "firmware=%s\n", furi_string_get_cstr(job->categories[Esp32FlashCategoryFirmware].path));
    furi_string_cat_printf(out, "custom_count=%zu\n", job->custom_count);
    for(size_t i = 0; i < job->custom_count; i++) {
        furi_string_cat_printf(out, "custom%zu_path=%s\n", i, furi_string_get_cstr(job->custom[i].path));
        furi_string_cat_printf(out, "custom%zu_offset=0x%lx\n", i, (unsigned long)job->custom[i].offset);
    }

    size_t len = furi_string_size(out);
    size_t written = storage_file_write(file, furi_string_get_cstr(out), len);
    furi_string_free(out);
    storage_file_close(file);
    storage_file_free(file);

    return written == len;
}
