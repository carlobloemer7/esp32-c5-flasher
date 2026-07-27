#include "../esp32_c5_flasher_i.h"
#include "../esp32/esp32_firmware_preset.h"

// Submenu item indices 0..Esp32FirmwarePresetCount-1 are the presets;
// the two values below extend that range for the remaining menu entries
// and the "dismiss error" custom event.
typedef enum {
    FirmwareSelectEventCustomFiles = Esp32FirmwarePresetCount,
    FirmwareSelectEventDismissError,
} FirmwareSelectEvent;

static void firmware_select_menu_callback(void* context, uint32_t index) {
    Esp32C5FlasherApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void firmware_select_show_menu(Esp32C5FlasherApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Firmware waehlen");

    for(size_t i = 0; i < Esp32FirmwarePresetCount; i++) {
        submenu_add_item(
            app->submenu,
            esp32_firmware_preset_name((Esp32FirmwarePreset)i),
            i,
            firmware_select_menu_callback,
            app);
    }
    submenu_add_item(
        app->submenu,
        "Eigene Dateien",
        FirmwareSelectEventCustomFiles,
        firmware_select_menu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, Esp32C5FlasherViewSubmenu);
}

static void firmware_select_error_button_callback(GuiButtonType result, InputType type, void* context) {
    UNUSED(result);
    Esp32C5FlasherApp* app = context;
    if(type != InputTypeShort) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, FirmwareSelectEventDismissError);
}

static void firmware_select_show_error(Esp32C5FlasherApp* app, const char* text) {
    widget_reset(app->widget);
    widget_add_text_box_element(app->widget, 0, 0, 128, 50, AlignLeft, AlignTop, text, false);
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "OK", firmware_select_error_button_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, Esp32C5FlasherViewWidget);
}

void esp32_c5_flasher_scene_firmware_select_on_enter(void* context) {
    firmware_select_show_menu(context);
}

bool esp32_c5_flasher_scene_firmware_select_on_event(void* context, SceneManagerEvent event) {
    Esp32C5FlasherApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == FirmwareSelectEventDismissError) {
        firmware_select_show_menu(app);
        return true;
    }

    if(event.event == FirmwareSelectEventCustomFiles) {
        // Use whatever is already configured via "Firmware verwalten" /
        // manually added custom binaries.
        scene_manager_next_scene(app->scene_manager, Esp32C5FlasherSceneConfirm);
        return true;
    }

    if(event.event < Esp32FirmwarePresetCount) {
        FuriString* error = furi_string_alloc();
        bool ok = esp32_firmware_preset_resolve(
            app->storage, (Esp32FirmwarePreset)event.event, app->flash_job, error);
        if(ok) {
            esp32_flash_job_save(app->flash_job, app->storage);
            scene_manager_next_scene(app->scene_manager, Esp32C5FlasherSceneConfirm);
        } else {
            firmware_select_show_error(app, furi_string_get_cstr(error));
        }
        furi_string_free(error);
        return true;
    }

    return false;
}

void esp32_c5_flasher_scene_firmware_select_on_exit(void* context) {
    Esp32C5FlasherApp* app = context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}
