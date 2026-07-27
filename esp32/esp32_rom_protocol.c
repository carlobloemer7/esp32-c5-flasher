#include "esp32_rom_protocol.h"

#include <stdlib.h>
#include <string.h>

// --- ESP32 ROM bootloader serial protocol -----------------------------------
// SLIP-framed request/response protocol, identical across ESP32 chips
// (implemented against the ROM loader directly, no stub uploaded - see the
// plan's risk notes on throughput). Reference: docs.espressif.com esptool
// "Serial Protocol" pages + espressif/esptool source (loader.py, cmds.py).

#define ESP_OP_FLASH_BEGIN 0x02
#define ESP_OP_FLASH_DATA 0x03
#define ESP_OP_FLASH_END 0x04
#define ESP_OP_SYNC 0x08
#define ESP_OP_SPI_ATTACH 0x0D
#define ESP_OP_CHANGE_BAUDRATE 0x0F

#define ESP_FLASH_BLOCK_SIZE 0x400u // ROM loader (no stub) fixed block size
#define ESP_CHECKSUM_MAGIC 0xEFu

#define ESP32_PROTO_HEADER_SIZE 8
#define ESP32_PROTO_FLASH_DATA_SUBHEADER_SIZE 16
// Header + FLASH_DATA subheader + one full data block - the largest packet
// this app ever sends.
#define ESP32_PROTO_RAW_BUF_SIZE \
    (ESP32_PROTO_HEADER_SIZE + ESP32_PROTO_FLASH_DATA_SUBHEADER_SIZE + ESP_FLASH_BLOCK_SIZE)
// Worst case SLIP escaping doubles every byte, plus the two frame delimiters.
#define ESP32_PROTO_ESCAPED_BUF_SIZE (ESP32_PROTO_RAW_BUF_SIZE * 2 + 2)

#define ESP32_SYNC_BAUD 115200u
#define ESP32_SYNC_RETRIES_PER_ATTEMPT 5
#define ESP32_SYNC_RESET_ATTEMPTS 7
#define ESP32_SYNC_TIMEOUT_MS 100

struct Esp32RomProtocol {
    Esp32Serial* serial;
    Esp32GpioStrap* strap;
    Storage* storage;

    uint8_t* raw_buf;
    uint8_t* escaped_buf;

    bool strap_initialized;
    bool auto_reset_active;
};

Esp32RomProtocol* esp32_rom_protocol_alloc(Storage* storage) {
    Esp32RomProtocol* instance = malloc(sizeof(Esp32RomProtocol));
    memset(instance, 0, sizeof(Esp32RomProtocol));
    instance->storage = storage;
    instance->serial = esp32_serial_alloc();
    instance->strap = esp32_gpio_strap_alloc();
    instance->raw_buf = malloc(ESP32_PROTO_RAW_BUF_SIZE);
    instance->escaped_buf = malloc(ESP32_PROTO_ESCAPED_BUF_SIZE);
    return instance;
}

void esp32_rom_protocol_free(Esp32RomProtocol* instance) {
    furi_check(instance);
    esp32_serial_free(instance->serial);
    esp32_gpio_strap_free(instance->strap);
    free(instance->raw_buf);
    free(instance->escaped_buf);
    free(instance);
}

static void put_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

// Builds the 8-byte header around whatever payload the caller has already
// written into raw_buf[8 .. 8+payload_len), SLIP-escapes the whole packet,
// and transmits it.
static bool send_raw_command(Esp32RomProtocol* instance, uint8_t opcode, uint16_t payload_len, uint32_t checksum) {
    uint8_t* raw = instance->raw_buf;
    raw[0] = 0x00; // direction: request
    raw[1] = opcode;
    raw[2] = (uint8_t)(payload_len & 0xFF);
    raw[3] = (uint8_t)((payload_len >> 8) & 0xFF);
    put_u32(raw + 4, checksum);

    size_t raw_len = ESP32_PROTO_HEADER_SIZE + payload_len;
    if(raw_len > ESP32_PROTO_RAW_BUF_SIZE) return false;

    size_t esc_len = 0;
    instance->escaped_buf[esc_len++] = 0xC0;
    for(size_t i = 0; i < raw_len; i++) {
        uint8_t b = raw[i];
        if(b == 0xDB) {
            instance->escaped_buf[esc_len++] = 0xDB;
            instance->escaped_buf[esc_len++] = 0xDD;
        } else if(b == 0xC0) {
            instance->escaped_buf[esc_len++] = 0xDB;
            instance->escaped_buf[esc_len++] = 0xDC;
        } else {
            instance->escaped_buf[esc_len++] = b;
        }
    }
    instance->escaped_buf[esc_len++] = 0xC0;

    esp32_serial_tx(instance->serial, instance->escaped_buf, esc_len);
    return true;
}

// Waits for and unescapes one SLIP frame (between two 0xC0 delimiters) into
// raw_out. Skips any noise/extra delimiters before the frame starts (the ROM
// bootloader can print plain-text boot logs before it starts speaking the
// protocol, and sends back-to-back 0xC0 bytes between some responses).
// Assumes a ~1kHz tick rate (furi_get_tick() step == 1ms), the firmware
// default.
static bool wait_and_unescape_frame(
    Esp32RomProtocol* instance,
    uint32_t timeout_ms,
    uint8_t* raw_out,
    size_t raw_out_cap,
    size_t* raw_len_out) {
    uint32_t start = furi_get_tick();
    uint8_t b;

    do {
        uint32_t elapsed = furi_get_tick() - start;
        if(elapsed >= timeout_ms) return false;
        if(!esp32_serial_read_byte_timeout(instance->serial, &b, timeout_ms - elapsed)) return false;
    } while(b != 0xC0);

    do {
        uint32_t elapsed = furi_get_tick() - start;
        if(elapsed >= timeout_ms) return false;
        if(!esp32_serial_read_byte_timeout(instance->serial, &b, timeout_ms - elapsed)) return false;
    } while(b == 0xC0);

    size_t n = 0;
    bool escape = false;
    while(b != 0xC0) {
        if(escape) {
            if(b == 0xDC) raw_out[n++] = 0xC0;
            else if(b == 0xDD) raw_out[n++] = 0xDB;
            else raw_out[n++] = b;
            escape = false;
        } else if(b == 0xDB) {
            escape = true;
        } else {
            raw_out[n++] = b;
        }
        if(n >= raw_out_cap) return false;

        uint32_t elapsed = furi_get_tick() - start;
        if(elapsed >= timeout_ms) return false;
        if(!esp32_serial_read_byte_timeout(instance->serial, &b, timeout_ms - elapsed)) return false;
    }

    *raw_len_out = n;
    return true;
}

// ROM loader responses (no stub) always end in 4 status bytes:
// [status, error, reserved, reserved].
static bool read_response(
    Esp32RomProtocol* instance,
    uint8_t expected_opcode,
    uint32_t timeout_ms,
    uint8_t* out_status) {
    uint8_t raw[ESP32_PROTO_RAW_BUF_SIZE];
    size_t raw_len;
    if(!wait_and_unescape_frame(instance, timeout_ms, raw, sizeof(raw), &raw_len)) return false;
    if(raw_len < ESP32_PROTO_HEADER_SIZE + 4) return false;

    uint8_t direction = raw[0];
    uint8_t opcode = raw[1];
    uint16_t size = (uint16_t)(raw[2] | (raw[3] << 8));
    if(direction != 0x01) return false;
    if(opcode != expected_opcode) return false;
    if(ESP32_PROTO_HEADER_SIZE + (size_t)size > raw_len) return false;
    if(size < 4) return false;

    if(out_status) *out_status = raw[ESP32_PROTO_HEADER_SIZE + size - 4];
    return true;
}

static bool cmd_sync(Esp32RomProtocol* instance) {
    uint8_t* p = instance->raw_buf + ESP32_PROTO_HEADER_SIZE;
    p[0] = 0x07;
    p[1] = 0x07;
    p[2] = 0x12;
    p[3] = 0x20;
    memset(p + 4, 0x55, 32);
    return send_raw_command(instance, ESP_OP_SYNC, 36, 0);
}

static bool try_sync_once(Esp32RomProtocol* instance) {
    if(!cmd_sync(instance)) return false;

    // The ROM answers a single SYNC with up to 8 near-identical responses;
    // take the first successful one and drain the rest best-effort.
    bool got_one = false;
    for(int i = 0; i < 8; i++) {
        uint8_t status = 0xFF;
        uint32_t timeout = got_one ? 50 : ESP32_SYNC_TIMEOUT_MS;
        if(read_response(instance, ESP_OP_SYNC, timeout, &status)) {
            if(status == 0) got_one = true;
        } else if(!got_one) {
            return false;
        } else {
            break;
        }
    }
    return got_one;
}

static bool cmd_change_baudrate(Esp32RomProtocol* instance, uint32_t new_baud) {
    uint8_t* p = instance->raw_buf + ESP32_PROTO_HEADER_SIZE;
    put_u32(p, new_baud);
    put_u32(p + 4, 0); // old baud, unused by ROM loader
    if(!send_raw_command(instance, ESP_OP_CHANGE_BAUDRATE, 8, 0)) return false;
    uint8_t status = 0xFF;
    return read_response(instance, ESP_OP_CHANGE_BAUDRATE, 500, &status) && status == 0;
}

static bool cmd_spi_attach(Esp32RomProtocol* instance) {
    uint8_t* p = instance->raw_buf + ESP32_PROTO_HEADER_SIZE;
    memset(p, 0, 8); // default SPI pins (embedded flash)
    if(!send_raw_command(instance, ESP_OP_SPI_ATTACH, 8, 0)) return false;
    uint8_t status = 0xFF;
    return read_response(instance, ESP_OP_SPI_ATTACH, 500, &status) && status == 0;
}

static bool cmd_flash_begin(Esp32RomProtocol* instance, uint32_t total_size, uint32_t offset) {
    uint32_t packet_count = (total_size + ESP_FLASH_BLOCK_SIZE - 1) / ESP_FLASH_BLOCK_SIZE;
    if(packet_count == 0) packet_count = 1;

    uint8_t* p = instance->raw_buf + ESP32_PROTO_HEADER_SIZE;
    put_u32(p + 0, total_size);
    put_u32(p + 4, packet_count);
    put_u32(p + 8, ESP_FLASH_BLOCK_SIZE);
    put_u32(p + 12, offset);
    put_u32(p + 16, 0); // encrypted-write flag: off

    if(!send_raw_command(instance, ESP_OP_FLASH_BEGIN, 20, 0)) return false;
    uint8_t status = 0xFF;
    // Erasing can take a while for larger images.
    return read_response(instance, ESP_OP_FLASH_BEGIN, 10000, &status) && status == 0;
}

// Caller must have already written `data_len` bytes of flash data into
// raw_buf[8+16 .. 8+16+data_len) before calling this.
static bool cmd_flash_data(Esp32RomProtocol* instance, uint16_t data_len, uint32_t seq) {
    uint8_t* p = instance->raw_buf + ESP32_PROTO_HEADER_SIZE;
    put_u32(p + 0, data_len);
    put_u32(p + 4, seq);
    put_u32(p + 8, 0);
    put_u32(p + 12, 0);

    const uint8_t* data = p + ESP32_PROTO_FLASH_DATA_SUBHEADER_SIZE;
    uint32_t checksum = ESP_CHECKSUM_MAGIC;
    for(uint16_t i = 0; i < data_len; i++) checksum ^= data[i];

    if(!send_raw_command(
           instance, ESP_OP_FLASH_DATA, (uint16_t)(ESP32_PROTO_FLASH_DATA_SUBHEADER_SIZE + data_len), checksum))
        return false;
    uint8_t status = 0xFF;
    return read_response(instance, ESP_OP_FLASH_DATA, 3000, &status) && status == 0;
}

static bool cmd_flash_end(Esp32RomProtocol* instance, bool reboot_into_app) {
    uint8_t* p = instance->raw_buf + ESP32_PROTO_HEADER_SIZE;
    put_u32(p, reboot_into_app ? 0 : 1);
    if(!send_raw_command(instance, ESP_OP_FLASH_END, 4, 0)) return false;
    uint8_t status = 0xFF;
    return read_response(instance, ESP_OP_FLASH_END, 500, &status) && status == 0;
}

Esp32FlashResult esp32_rom_protocol_connect(
    Esp32RomProtocol* instance,
    uint32_t baud,
    bool auto_reset,
    bool gpio27_assist,
    const volatile bool* abort_flag) {
    if(!esp32_serial_open(instance->serial, ESP32_SYNC_BAUD)) {
        return Esp32FlashResultProtocolError;
    }

    instance->auto_reset_active = auto_reset;
    if(auto_reset) {
        esp32_gpio_strap_init(instance->strap, gpio27_assist);
        instance->strap_initialized = true;
    }

    for(int attempt = 0; attempt < ESP32_SYNC_RESET_ATTEMPTS; attempt++) {
        if(abort_flag && *abort_flag) return Esp32FlashResultAborted;

        if(auto_reset) {
            esp32_gpio_strap_enter_bootloader(instance->strap);
        }
        esp32_serial_flush_rx(instance->serial);

        for(int retry = 0; retry < ESP32_SYNC_RETRIES_PER_ATTEMPT; retry++) {
            if(abort_flag && *abort_flag) return Esp32FlashResultAborted;

            if(try_sync_once(instance)) {
                if(baud != ESP32_SYNC_BAUD) {
                    if(cmd_change_baudrate(instance, baud)) {
                        esp32_serial_set_baud(instance->serial, baud);
                        esp32_serial_flush_rx(instance->serial);
                    }
                    // best-effort: if it fails, we simply continue at 115200
                }
                return Esp32FlashResultOk;
            }
            furi_delay_ms(50);
        }

        if(!auto_reset) {
            // Manual mode: give the user a moment to hold BOOT and tap RESET
            // between rounds.
            furi_delay_ms(300);
        }
    }

    return Esp32FlashResultSyncTimeout;
}

Esp32FlashResult esp32_rom_protocol_flash_job(
    Esp32RomProtocol* instance,
    const Esp32FlashJob* job,
    Esp32FlashProgressCallback progress_cb,
    void* progress_context,
    const volatile bool* abort_flag) {
    if(!cmd_spi_attach(instance)) return Esp32FlashResultProtocolError;

    size_t total_entries = esp32_flash_job_resolved_count(job);
    if(total_entries == 0) return Esp32FlashResultFileError;

    FuriString* path = furi_string_alloc();
    Esp32FlashResult result = Esp32FlashResultOk;

    for(size_t i = 0; i < total_entries; i++) {
        if(abort_flag && *abort_flag) {
            result = Esp32FlashResultAborted;
            break;
        }

        uint32_t offset = 0;
        if(!esp32_flash_job_resolved_get(job, i, path, &offset)) {
            result = Esp32FlashResultFileError;
            break;
        }

        File* file = storage_file_alloc(instance->storage);
        if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            storage_file_free(file);
            result = Esp32FlashResultFileError;
            break;
        }
        uint64_t file_size = storage_file_size(file);

        if(progress_cb) {
            Esp32FlashProgressEvent ev = {
                .type = Esp32FlashProgressStartFile,
                .file_name = furi_string_get_cstr(path),
                .file_index = (uint8_t)i,
                .total_files = (uint8_t)total_entries,
                .file_percent = 0,
                .overall_percent = (uint8_t)((i * 100) / total_entries),
            };
            progress_cb(&ev, progress_context);
        }

        if(!cmd_flash_begin(instance, (uint32_t)file_size, offset)) {
            storage_file_close(file);
            storage_file_free(file);
            result = Esp32FlashResultProtocolError;
            break;
        }

        uint32_t seq = 0;
        uint64_t written = 0;
        bool file_ok = true;

        while(written < file_size) {
            if(abort_flag && *abort_flag) {
                result = Esp32FlashResultAborted;
                file_ok = false;
                break;
            }

            uint64_t remaining = file_size - written;
            size_t chunk = remaining < ESP_FLASH_BLOCK_SIZE ? (size_t)remaining : ESP_FLASH_BLOCK_SIZE;

            uint8_t* data_dst = instance->raw_buf + ESP32_PROTO_HEADER_SIZE + ESP32_PROTO_FLASH_DATA_SUBHEADER_SIZE;
            size_t got = storage_file_read(file, data_dst, chunk);
            if(got != chunk) {
                result = Esp32FlashResultFileError;
                file_ok = false;
                break;
            }
            // Pad a short final block with 0xFF (matches post-erase flash
            // content) so the chip always receives full-size packets.
            if(chunk < ESP_FLASH_BLOCK_SIZE) {
                memset(data_dst + chunk, 0xFF, ESP_FLASH_BLOCK_SIZE - chunk);
            }

            if(!cmd_flash_data(instance, ESP_FLASH_BLOCK_SIZE, seq)) {
                result = Esp32FlashResultProtocolError;
                file_ok = false;
                break;
            }

            seq++;
            written += got;

            if(progress_cb) {
                uint8_t file_percent = (uint8_t)((written * 100) / file_size);
                uint8_t overall_percent = (uint8_t)(((i * 100) + file_percent) / total_entries);
                Esp32FlashProgressEvent ev = {
                    .type = Esp32FlashProgressFileProgress,
                    .file_name = furi_string_get_cstr(path),
                    .file_index = (uint8_t)i,
                    .total_files = (uint8_t)total_entries,
                    .file_percent = file_percent,
                    .overall_percent = overall_percent,
                };
                progress_cb(&ev, progress_context);
            }
        }

        storage_file_close(file);
        storage_file_free(file);

        if(!file_ok) break;

        if(!cmd_flash_end(instance, false)) {
            result = Esp32FlashResultProtocolError;
            break;
        }

        if(progress_cb) {
            Esp32FlashProgressEvent ev = {
                .type = Esp32FlashProgressFileDone,
                .file_name = furi_string_get_cstr(path),
                .file_index = (uint8_t)i,
                .total_files = (uint8_t)total_entries,
                .file_percent = 100,
                .overall_percent = (uint8_t)(((i + 1) * 100) / total_entries),
            };
            progress_cb(&ev, progress_context);
        }
    }

    furi_string_free(path);
    return result;
}

void esp32_rom_protocol_disconnect(Esp32RomProtocol* instance) {
    if(instance->auto_reset_active) {
        // Boot into the freshly flashed app.
        esp32_gpio_strap_hard_reset(instance->strap);
    }
    if(instance->strap_initialized) {
        esp32_gpio_strap_deinit(instance->strap);
        instance->strap_initialized = false;
    }
    instance->auto_reset_active = false;
    esp32_serial_close(instance->serial);
}
