#include "../esp32_c5_flasher_i.h"

#include <string.h>

typedef enum {
    ConfirmEventStart,
    ConfirmEventBack,
} ConfirmEvent;

static const char* basename_of(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void confirm_button_callback(GuiButtonType result, InputType type, void* context) {
    Esp32C5FlasherApp* app = context;
    if(type != InputTypeShort) return;

    if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ConfirmEventStart);
    } else if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ConfirmEventBack);
    }
}

void esp32_c5_flasher_scene_confirm_on_enter(void* context) {
    Esp32C5FlasherApp* app = context;
    Esp32FlashJob* job = app->flash_job;

    widget_reset(app->widget);

    bool ready = esp32_flash_job_is_ready(job);
    size_t count = esp32_flash_job_resolved_count(job);

    FuriString* text = furi_string_alloc();
    if(!ready) {
        furi_string_cat_printf(
            text, "Fehlt noch: Bootloader, Partitionstabelle und Firmware muessen gesetzt sein.\n");
        furi_string_cat_printf(text, "Siehe 'Firmware verwalten'.");
    } else {
        FuriString* path = furi_string_alloc();
        for(size_t i = 0; i < count; i++) {
            uint32_t offset;
            esp32_flash_job_resolved_get(job, i, path, &offset);
            furi_string_cat_printf(
                text, "0x%05lX: %s\n", (unsigned long)offset, basename_of(furi_string_get_cstr(path)));
        }
        furi_string_free(path);
    }

    widget_add_text_box_element(
        app->widget, 0, 0, 128, 50, AlignLeft, AlignTop, furi_string_get_cstr(text), false);
    furi_string_free(text);

    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Zurueck", confirm_button_callback, app);
    if(ready) {
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Start", confirm_button_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, Esp32C5FlasherViewWidget);
}

bool esp32_c5_flasher_scene_confirm_on_event(void* context, SceneManagerEvent event) {
    Esp32C5FlasherApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == ConfirmEventStart) {
        scene_manager_next_scene(app->scene_manager, Esp32C5FlasherSceneFlashing);
        return true;
    } else if(event.event == ConfirmEventBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void esp32_c5_flasher_scene_confirm_on_exit(void* context) {
    Esp32C5FlasherApp* app = context;
    widget_reset(app->widget);
}
