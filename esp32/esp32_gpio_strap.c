#include "esp32_gpio_strap.h"

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include <stdlib.h>

// Timing taken from esptool.py's classic-reset defaults (DEFAULT_RESET_DELAY
// = 50ms boot-hold, 100ms reset-hold). Not independently verified against
// ESP32-C5 silicon - adjust here first if auto-reset proves unreliable.
#define ESP32_STRAP_RESET_HOLD_MS 100
#define ESP32_STRAP_BOOT_HOLD_MS 50

struct Esp32GpioStrap {
    bool gpio27_assist;
};

Esp32GpioStrap* esp32_gpio_strap_alloc(void) {
    Esp32GpioStrap* instance = malloc(sizeof(Esp32GpioStrap));
    instance->gpio27_assist = false;
    return instance;
}

void esp32_gpio_strap_free(Esp32GpioStrap* instance) {
    furi_check(instance);
    free(instance);
}

void esp32_gpio_strap_init(Esp32GpioStrap* instance, bool gpio27_assist) {
    furi_assert(instance);
    instance->gpio27_assist = gpio27_assist;

    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh); // EN
    furi_hal_gpio_init(
        &gpio_ext_pb2, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh); // BOOT/GPIO28
    furi_hal_gpio_write(&gpio_ext_pc3, true); // EN idle HIGH - not in reset
    furi_hal_gpio_write(&gpio_ext_pb2, true); // BOOT idle HIGH - normal boot select

    if(gpio27_assist) {
        furi_hal_gpio_init(
            &gpio_ext_pa4, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh); // GPIO27 assist
        furi_hal_gpio_write(&gpio_ext_pa4, true);
    }
}

void esp32_gpio_strap_deinit(Esp32GpioStrap* instance) {
    furi_assert(instance);
    furi_hal_gpio_init_simple(&gpio_ext_pc3, GpioModeInput);
    furi_hal_gpio_init_simple(&gpio_ext_pb2, GpioModeInput);
    if(instance->gpio27_assist) {
        furi_hal_gpio_init_simple(&gpio_ext_pa4, GpioModeInput);
    }
}

void esp32_gpio_strap_enter_bootloader(Esp32GpioStrap* instance) {
    UNUSED(instance);

    furi_hal_gpio_write(&gpio_ext_pb2, true); // BOOT high
    furi_hal_gpio_write(&gpio_ext_pc3, false); // EN low -> hold chip in reset
    furi_delay_ms(ESP32_STRAP_RESET_HOLD_MS);

    furi_hal_gpio_write(&gpio_ext_pb2, false); // BOOT low -> select UART download mode
    furi_hal_gpio_write(&gpio_ext_pc3, true); // EN high -> release reset, chip boots & samples straps
    furi_delay_ms(ESP32_STRAP_BOOT_HOLD_MS);

    furi_hal_gpio_write(&gpio_ext_pb2, true); // release BOOT back high
}

void esp32_gpio_strap_hard_reset(Esp32GpioStrap* instance) {
    UNUSED(instance);
    furi_hal_gpio_write(&gpio_ext_pc3, false);
    furi_delay_ms(ESP32_STRAP_RESET_HOLD_MS);
    furi_hal_gpio_write(&gpio_ext_pc3, true);
}
