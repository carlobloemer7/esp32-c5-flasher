#pragma once

#include <furi.h>
#include <storage/storage.h>

#include "esp32_flash_job.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    Esp32FirmwarePresetGhostEsp,
    Esp32FirmwarePresetMarauder,
    Esp32FirmwarePresetFlipperHttp,
    Esp32FirmwarePresetWardriver,
    Esp32FirmwarePresetCount,
} Esp32FirmwarePreset;

const char* esp32_firmware_preset_name(Esp32FirmwarePreset preset);
const char* esp32_firmware_preset_folder(Esp32FirmwarePreset preset);

// Scans the preset's folder for bootloader.bin, partition-table.bin (or
// partitions.bin) and ota_data_initial.bin by exact name, then picks the
// largest remaining .bin in that folder as the app image - the same
// detection convention AWOK559's C5_Py_Flasher uses. On success, fills
// job->categories[Bootloader/PartitionTable/OtaData/Firmware] (does not
// touch job->custom) and returns true. On failure (missing bootloader or
// partition table), returns false and writes an explanation - including
// which folder to drop files into - to out_error.
bool esp32_firmware_preset_resolve(
    Storage* storage,
    Esp32FirmwarePreset preset,
    Esp32FlashJob* job,
    FuriString* out_error);

#ifdef __cplusplus
}
#endif
