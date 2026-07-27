#include "esp32_serial.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>
#include <furi/core/stream_buffer.h>

#include <stdlib.h>
#include <string.h>

#define ESP32_SERIAL_RX_STREAM_SIZE 2048

struct Esp32Serial {
    FuriHalSerialHandle* handle;
    FuriStreamBuffer* rx_stream;
    Expansion* expansion;
    bool open;
};

// NOTE: the current firmware's FuriHalSerialAsyncRxCallback signature is the
// most likely spot for an SDK-version mismatch (see plan risk notes). This
// assumes the event-based signature (report_errors flag selects which
// FuriHalSerialRxEvent bits fire; fetch the byte via furi_hal_serial_async_rx()
// on a Data event) used by current firmware. If your SDK instead calls back
// with the raw byte directly, adjust this function's signature and drop the
// furi_hal_serial_async_rx() call accordingly - the fix is local to this file.
static void esp32_serial_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    Esp32Serial* instance = context;
    if(event & FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(instance->rx_stream, &data, 1, 0);
    }
}

Esp32Serial* esp32_serial_alloc(void) {
    Esp32Serial* instance = malloc(sizeof(Esp32Serial));
    memset(instance, 0, sizeof(Esp32Serial));
    instance->rx_stream = furi_stream_buffer_alloc(ESP32_SERIAL_RX_STREAM_SIZE, 1);
    return instance;
}

void esp32_serial_close(Esp32Serial* instance) {
    furi_assert(instance);
    if(!instance->open) return;

    furi_hal_serial_async_rx_stop(instance->handle);
    furi_hal_serial_deinit(instance->handle);
    furi_hal_serial_control_release(instance->handle);
    instance->handle = NULL;

    if(instance->expansion) {
        expansion_enable(instance->expansion);
        furi_record_close(RECORD_EXPANSION);
        instance->expansion = NULL;
    }

    instance->open = false;
}

void esp32_serial_free(Esp32Serial* instance) {
    furi_check(instance);
    esp32_serial_close(instance);
    furi_stream_buffer_free(instance->rx_stream);
    free(instance);
}

bool esp32_serial_open(Esp32Serial* instance, uint32_t baud) {
    furi_assert(instance);
    if(instance->open) return true;
    if(furi_hal_serial_control_is_busy(FuriHalSerialIdUsart)) return false;

    instance->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(instance->expansion);

    instance->handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!instance->handle) {
        expansion_enable(instance->expansion);
        furi_record_close(RECORD_EXPANSION);
        instance->expansion = NULL;
        return false;
    }

    furi_hal_serial_init(instance->handle, baud);
    esp32_serial_flush_rx(instance);
    furi_hal_serial_async_rx_start(instance->handle, esp32_serial_rx_callback, instance, false);

    instance->open = true;
    return true;
}

void esp32_serial_set_baud(Esp32Serial* instance, uint32_t baud) {
    furi_assert(instance);
    furi_assert(instance->open);
    furi_hal_serial_set_br(instance->handle, baud);
}

void esp32_serial_tx(Esp32Serial* instance, const uint8_t* data, size_t len) {
    furi_assert(instance);
    furi_assert(instance->open);
    furi_hal_serial_tx(instance->handle, data, len);
    furi_hal_serial_tx_wait_complete(instance->handle);
}

size_t esp32_serial_read_timeout(
    Esp32Serial* instance,
    uint8_t* data,
    size_t len,
    uint32_t timeout_ms) {
    furi_assert(instance);
    furi_assert(instance->open);

    uint32_t start_tick = furi_get_tick();
    uint32_t timeout_ticks = furi_ms_to_ticks(timeout_ms);
    size_t total = 0;

    while(total < len) {
        uint32_t elapsed = furi_get_tick() - start_tick;
        if(elapsed >= timeout_ticks) break;
        size_t got = furi_stream_buffer_receive(
            instance->rx_stream, data + total, len - total, timeout_ticks - elapsed);
        if(got == 0) break;
        total += got;
    }

    return total;
}

bool esp32_serial_read_byte_timeout(Esp32Serial* instance, uint8_t* out_byte, uint32_t timeout_ms) {
    return esp32_serial_read_timeout(instance, out_byte, 1, timeout_ms) == 1;
}

void esp32_serial_flush_rx(Esp32Serial* instance) {
    furi_assert(instance);
    uint8_t tmp[64];
    while(furi_stream_buffer_receive(instance->rx_stream, tmp, sizeof(tmp), 0) > 0) {
        // discard buffered bytes
    }
}
