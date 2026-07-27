#pragma once

#include <furi.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    Esp32FlashCategoryBootloader,
    Esp32FlashCategoryPartitionTable,
    Esp32FlashCategoryOtaData,
    Esp32FlashCategoryFirmware,
    Esp32FlashCategoryCount,
} Esp32FlashCategory;

#define ESP32_CUSTOM_ENTRIES_MAX 8

typedef struct {
    FuriString* path; // empty string = unset
    uint32_t offset;
} Esp32FlashEntry;

typedef struct {
    Esp32FlashEntry categories[Esp32FlashCategoryCount];
    Esp32FlashEntry custom[ESP32_CUSTOM_ENTRIES_MAX];
    size_t custom_count;

    bool auto_reset_enabled;
    bool gpio27_assist_enabled;
    uint32_t baud_rate;
} Esp32FlashJob;

Esp32FlashJob* esp32_flash_job_alloc(void);
void esp32_flash_job_free(Esp32FlashJob* job);

// Loads config.txt if present; missing file / fields keep the struct's
// existing (default) values.
void esp32_flash_job_load(Esp32FlashJob* job, Storage* storage);
bool esp32_flash_job_save(Esp32FlashJob* job, Storage* storage);

// Ensures the standard data folders (bootloader/, partition_table/, ...)
// exist on the SD card. Safe to call every launch.
void esp32_flash_job_ensure_folders(Storage* storage);

const char* esp32_flash_job_category_folder(Esp32FlashCategory category);
const char* esp32_flash_job_category_name(Esp32FlashCategory category);
uint32_t esp32_flash_job_category_offset(Esp32FlashCategory category);
// Only ota_data is optional; the other three categories are required for a
// bootable flash job.
bool esp32_flash_job_category_optional(Esp32FlashCategory category);

void esp32_flash_job_set_category_path(
    Esp32FlashJob* job,
    Esp32FlashCategory category,
    const char* path);

// Same as esp32_flash_job_set_category_path but also overrides the entry's
// flash offset. Needed because not all firmware projects use the same
// bootloader offset (e.g. GhostESP/FlipperHTTP flash their ESP32-C5
// bootloader at 0x0, while Marauder/Wardriver use the ESP-IDF-standard
// 0x2000) - partition-table/ota_data/app offsets are consistent across all
// of them so only bootloader realistically needs this.
void esp32_flash_job_set_category_entry(
    Esp32FlashJob* job,
    Esp32FlashCategory category,
    const char* path,
    uint32_t offset);

// Returns false if the job already has ESP32_CUSTOM_ENTRIES_MAX entries.
bool esp32_flash_job_add_custom(Esp32FlashJob* job, const char* path, uint32_t offset);
void esp32_flash_job_remove_custom(Esp32FlashJob* job, size_t index);

// Number of entries that will actually be flashed (categories with a path
// set, plus all custom entries).
size_t esp32_flash_job_resolved_count(const Esp32FlashJob* job);
// entry_index is 0-based across the resolved set (categories first, in
// Esp32FlashCategory order, then custom entries in insertion order).
bool esp32_flash_job_resolved_get(
    const Esp32FlashJob* job,
    size_t entry_index,
    FuriString* out_path,
    uint32_t* out_offset);

// Bootloader + partition table + firmware must all have a path assigned.
bool esp32_flash_job_is_ready(const Esp32FlashJob* job);

#ifdef __cplusplus
}
#endif
