#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi/core/thread.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include "scenes/esp32_c5_flasher_scene.h"
#include "views/flash_progress_view.h"
#include "esp32/esp32_flash_job.h"
#include "esp32/esp32_firmware_preset.h"

typedef enum {
    Esp32C5FlasherViewSubmenu,
    Esp32C5FlasherViewVariableItemList,
    Esp32C5FlasherViewByteInput,
    Esp32C5FlasherViewWidget,
    Esp32C5FlasherViewFlashProgress,
} Esp32C5FlasherView;

typedef struct {
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Storage* storage;
    DialogsApp* dialogs;

    Submenu* submenu;
    VariableItemList* variable_item_list;
    ByteInput* byte_input;
    Widget* widget;
    FlashProgressView* flash_progress_view;

    // Scratch buffers shared across scenes for the file-browser + offset-entry flow.
    FuriString* file_path;
    uint8_t offset_bytes[4];

    Esp32FlashJob* flash_job;
    // Category currently being edited by the file_category scene.
    Esp32FlashCategory active_category;
    // Path picked by add_custom_file, waiting for add_custom_offset to attach an offset.
    FuriString* pending_custom_path;

    // Set by the flashing scene worker thread when the job finishes/fails, so
    // the scene's on_event (running on the UI thread) can react to it.
    FuriThread* flash_thread;
    volatile bool flash_abort_requested;
} Esp32C5FlasherApp;
