#include "esp32_firmware_preset.h"
#include "esp32_paths.h"

#include <string.h>
#include <stdlib.h>

static const char* const preset_names[Esp32FirmwarePresetCount] = {
    [Esp32FirmwarePresetGhostEsp] = "GhostESP",
    [Esp32FirmwarePresetMarauder] = "Marauder",
    [Esp32FirmwarePresetFlipperHttp] = "FlipperHTTP",
    [Esp32FirmwarePresetWardriver] = "Wardriver",
};

static const char* const preset_folders[Esp32FirmwarePresetCount] = {
    [Esp32FirmwarePresetGhostEsp] = ESP32_FIRMWARES_FOLDER "/ghostesp",
    [Esp32FirmwarePresetMarauder] = ESP32_FIRMWARES_FOLDER "/marauder",
    [Esp32FirmwarePresetFlipperHttp] = ESP32_FIRMWARES_FOLDER "/flipperhttp",
    [Esp32FirmwarePresetWardriver] = ESP32_FIRMWARES_FOLDER "/wardriver",
};

// Not all of these projects agree on where the bootloader goes: GhostESP and
// FlipperHTTP both build/flash their ESP32-C5 bootloader at 0x0, while
// Marauder and Wardriver use the ESP-IDF-standard 0x2000 (same as
// esptool's own BOOTLOADER_FLASH_OFFSET for esp32c5). Partition-table
// (0x8000), ota_data (0xd000) and app (0x10000) are consistent across all
// four, so only this one needs a per-preset override.
static const uint32_t preset_bootloader_offsets[Esp32FirmwarePresetCount] = {
    [Esp32FirmwarePresetGhostEsp] = 0x0,
    [Esp32FirmwarePresetMarauder] = 0x2000,
    [Esp32FirmwarePresetFlipperHttp] = 0x0,
    [Esp32FirmwarePresetWardriver] = 0x2000,
};

const char* esp32_firmware_preset_name(Esp32FirmwarePreset preset) {
    furi_assert(preset < Esp32FirmwarePresetCount);
    return preset_names[preset];
}

const char* esp32_firmware_preset_folder(Esp32FirmwarePreset preset) {
    furi_assert(preset < Esp32FirmwarePresetCount);
    return preset_folders[preset];
}

static bool file_exists(Storage* storage, const char* path) {
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK && !(info.flags & FSF_DIRECTORY);
}

// Finds the largest ".bin" file directly inside `folder` whose full path
// doesn't match any of the (already-claimed) exclude paths. Returns false if
// none found.
static bool find_largest_other_bin(
    Storage* storage,
    const char* folder,
    const char* exclude_a,
    const char* exclude_b,
    const char* exclude_c,
    FuriString* out_path) {
    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, folder)) {
        storage_file_free(dir);
        return false;
    }

    FuriString* candidate = furi_string_alloc();
    char name[256];
    FileInfo info;
    uint64_t best_size = 0;
    bool found = false;

    while(storage_dir_read(dir, &info, name, sizeof(name))) {
        if(info.flags & FSF_DIRECTORY) continue;

        size_t len = strlen(name);
        if(len < 4) continue;
        const char* dot = name + len - 4;
        if(strcasecmp(dot, ".bin") != 0) continue;

        furi_string_printf(candidate, "%s/%s", folder, name);
        const char* cpath = furi_string_get_cstr(candidate);
        if((exclude_a && strcmp(cpath, exclude_a) == 0) ||
           (exclude_b && strcmp(cpath, exclude_b) == 0) ||
           (exclude_c && strcmp(cpath, exclude_c) == 0)) {
            continue;
        }

        if(!found || info.size > best_size) {
            best_size = info.size;
            furi_string_set(out_path, candidate);
            found = true;
        }
    }

    furi_string_free(candidate);
    storage_dir_close(dir);
    storage_file_free(dir);
    return found;
}

bool esp32_firmware_preset_resolve(
    Storage* storage,
    Esp32FirmwarePreset preset,
    Esp32FlashJob* job,
    FuriString* out_error) {
    const char* folder = esp32_firmware_preset_folder(preset);

    FuriString* bootloader = furi_string_alloc_printf("%s/bootloader.bin", folder);
    FuriString* partitions = furi_string_alloc_printf("%s/partition-table.bin", folder);
    if(!file_exists(storage, furi_string_get_cstr(partitions))) {
        furi_string_printf(partitions, "%s/partitions.bin", folder);
    }
    FuriString* ota_data = furi_string_alloc_printf("%s/ota_data_initial.bin", folder);

    bool have_bootloader = file_exists(storage, furi_string_get_cstr(bootloader));
    bool have_partitions = file_exists(storage, furi_string_get_cstr(partitions));
    bool have_ota_data = file_exists(storage, furi_string_get_cstr(ota_data));

    bool ok = have_bootloader && have_partitions;
    if(!ok) {
        furi_string_printf(
            out_error,
            "%s: bootloader.bin und/oder partition-table.bin fehlen in\n%s\n"
            "Das sollte beim App-Start automatisch entpackt werden - App ggf. neu installieren.",
            esp32_firmware_preset_name(preset),
            folder);
    } else {
        FuriString* app = furi_string_alloc();
        bool have_app = find_largest_other_bin(
            storage,
            folder,
            furi_string_get_cstr(bootloader),
            furi_string_get_cstr(partitions),
            have_ota_data ? furi_string_get_cstr(ota_data) : NULL,
            app);

        if(!have_app) {
            ok = false;
            furi_string_printf(
                out_error,
                "%s: keine App-/Firmware-.bin gefunden in\n%s",
                esp32_firmware_preset_name(preset),
                folder);
        } else {
            esp32_flash_job_set_category_entry(
                job,
                Esp32FlashCategoryBootloader,
                furi_string_get_cstr(bootloader),
                preset_bootloader_offsets[preset]);
            esp32_flash_job_set_category_path(
                job, Esp32FlashCategoryPartitionTable, furi_string_get_cstr(partitions));
            esp32_flash_job_set_category_path(
                job,
                Esp32FlashCategoryOtaData,
                have_ota_data ? furi_string_get_cstr(ota_data) : "");
            esp32_flash_job_set_category_path(
                job, Esp32FlashCategoryFirmware, furi_string_get_cstr(app));
        }
        furi_string_free(app);
    }

    furi_string_free(bootloader);
    furi_string_free(partitions);
    furi_string_free(ota_data);

    return ok;
}
