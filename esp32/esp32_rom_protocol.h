#pragma once

#include <furi.h>
#include <storage/storage.h>

#include "esp32_serial.h"
#include "esp32_gpio_strap.h"
#include "esp32_flash_job.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    Esp32FlashResultOk,
    Esp32FlashResultSyncTimeout,
    Esp32FlashResultProtocolError,
    Esp32FlashResultFileError,
    Esp32FlashResultAborted,
} Esp32FlashResult;

typedef enum {
    Esp32FlashProgressStartFile,
    Esp32FlashProgressFileProgress,
    Esp32FlashProgressFileDone,
} Esp32FlashProgressEventType;

typedef struct {
    Esp32FlashProgressEventType type;
    const char* file_name;
    uint8_t file_index;
    uint8_t total_files;
    uint8_t file_percent;
    uint8_t overall_percent;
} Esp32FlashProgressEvent;

typedef void (*Esp32FlashProgressCallback)(const Esp32FlashProgressEvent* event, void* context);

typedef struct Esp32RomProtocol Esp32RomProtocol;

Esp32RomProtocol* esp32_rom_protocol_alloc(Storage* storage);
void esp32_rom_protocol_free(Esp32RomProtocol* instance);

// Opens the UART (at 115200 for the initial handshake, as required by the
// ROM loader) and repeatedly attempts SYNC. In auto-reset mode this also
// drives EN/BOOT to force the chip into the download bootloader before each
// attempt; in manual mode the caller is expected to prompt the user to do
// this by hand between retries. On success, best-effort switches to `baud`
// via CHANGE_BAUDRATE (falls back to staying at 115200 if that fails).
// abort_flag is polled between retries and may be NULL.
Esp32FlashResult esp32_rom_protocol_connect(
    Esp32RomProtocol* instance,
    uint32_t baud,
    bool auto_reset,
    bool gpio27_assist,
    const volatile bool* abort_flag);

// Flashes every resolved entry of `job` in order. Must be called after a
// successful esp32_rom_protocol_connect(). abort_flag may be NULL.
Esp32FlashResult esp32_rom_protocol_flash_job(
    Esp32RomProtocol* instance,
    const Esp32FlashJob* job,
    Esp32FlashProgressCallback progress_cb,
    void* progress_context,
    const volatile bool* abort_flag);

// Reboots the chip into the newly flashed app (hardware EN pulse, only
// meaningful if connect() was called with auto_reset=true) and releases the
// UART/GPIO. Always call this once after connect() succeeded, whether or not
// flash_job() itself succeeded.
void esp32_rom_protocol_disconnect(Esp32RomProtocol* instance);

#ifdef __cplusplus
}
#endif
