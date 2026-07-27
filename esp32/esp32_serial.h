#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Esp32Serial Esp32Serial;

Esp32Serial* esp32_serial_alloc(void);
void esp32_serial_free(Esp32Serial* instance);

// Acquires the hardware USART (header pins 13/14), disabling the Expansion
// Module protocol first so it doesn't fight over the same pins. Returns
// false if the USART is already in use by something else.
bool esp32_serial_open(Esp32Serial* instance, uint32_t baud);
void esp32_serial_close(Esp32Serial* instance);

void esp32_serial_set_baud(Esp32Serial* instance, uint32_t baud);
void esp32_serial_tx(Esp32Serial* instance, const uint8_t* data, size_t len);

// Blocks until either `len` bytes have been read or `timeout_ms` has elapsed
// since the call started. Returns the number of bytes actually read.
size_t esp32_serial_read_timeout(
    Esp32Serial* instance,
    uint8_t* data,
    size_t len,
    uint32_t timeout_ms);

// Reads a single byte, blocking up to timeout_ms. Returns false on timeout.
bool esp32_serial_read_byte_timeout(Esp32Serial* instance, uint8_t* out_byte, uint32_t timeout_ms);

// Discards any buffered, not-yet-read RX bytes.
void esp32_serial_flush_rx(Esp32Serial* instance);

#ifdef __cplusplus
}
#endif
