#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Esp32GpioStrap Esp32GpioStrap;

Esp32GpioStrap* esp32_gpio_strap_alloc(void);
void esp32_gpio_strap_free(Esp32GpioStrap* instance);

// Configures EN (header pin 7 / PC3) and BOOT (header pin 6 / PB2) as
// push-pull outputs, both idle HIGH (chip not held in reset, boot-select not
// asserted). If gpio27_assist is true, also drives header pin 4 (PA4) HIGH
// for the ESP32-C5's second strap pin (GPIO27) - see the app's Settings
// screen; most WROOM-1U boards keep GPIO27 high via its internal pull-up on
// their own, so this defaults to off.
void esp32_gpio_strap_init(Esp32GpioStrap* instance, bool gpio27_assist);
// Returns the pins to inputs so they don't interfere when unused.
void esp32_gpio_strap_deinit(Esp32GpioStrap* instance);

// Classic reset-into-UART-download-bootloader sequence, translated from
// esptool.py's DTR/RTS dance to direct (non-inverted) GPIO drive:
//   BOOT=HIGH, EN=LOW   (hold in reset)      wait 100ms
//   BOOT=LOW,  EN=HIGH  (release, samples straps) wait 50ms
//   BOOT=HIGH           (release strap pin)
void esp32_gpio_strap_enter_bootloader(Esp32GpioStrap* instance);

// Plain reset pulse on EN only (BOOT left alone) - boots the chip normally
// into whatever is in flash.
void esp32_gpio_strap_hard_reset(Esp32GpioStrap* instance);

#ifdef __cplusplus
}
#endif
