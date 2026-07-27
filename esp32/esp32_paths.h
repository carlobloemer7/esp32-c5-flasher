#pragma once

#include <storage/storage.h> // for APP_DATA_PATH()

// All paths are rooted at the app's data folder: /ext/apps_data/esp32_c5_flasher
#define ESP32_APP_DATA_FOLDER APP_DATA_PATH("")
#define ESP32_BOOTLOADER_FOLDER APP_DATA_PATH("bootloader")
#define ESP32_PARTITION_TABLE_FOLDER APP_DATA_PATH("partition_table")
#define ESP32_OTA_DATA_FOLDER APP_DATA_PATH("ota_data")
#define ESP32_FIRMWARE_FOLDER APP_DATA_PATH("firmware")
#define ESP32_CUSTOM_FOLDER APP_DATA_PATH("custom")
#define ESP32_CONFIG_FILE APP_DATA_PATH("config.txt")

// Preset firmwares (GhostESP/Marauder/FlipperHTTP/Wardriver) ship bundled
// inside the .fap itself (fap_file_assets="resources" in application.fam)
// and get auto-extracted here by the OS on first launch - read-only from
// the app's point of view. "Eigene Dateien" (custom bins) stay in
// apps_data above, since those are user-provided/writable.
#define ESP32_FIRMWARES_FOLDER APP_ASSETS_PATH("firmwares")

#define ESP32_FILE_EXTENSION ".bin"

// Fixed flash offsets for the standard ESP-IDF partition layout on RISC-V
// chips (esp32c3/c5/c6/h2): bootloader 0x2000, partition table 0x8000,
// ota_data_initial 0xd000, app 0x10000.
#define ESP32_OFFSET_BOOTLOADER 0x2000u
#define ESP32_OFFSET_PARTITION_TABLE 0x8000u
#define ESP32_OFFSET_OTA_DATA 0xd000u
#define ESP32_OFFSET_FIRMWARE 0x10000u
