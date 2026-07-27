#include "../esp32_c5_flasher_i.h"

#include <string.h>

static void add_custom_offset_result_callback(void* context) {
    Esp32C5FlasherApp* app = context;

    uint32_t offset = ((uint32_t)app->offset_bytes[0] << 24) | ((uint32_t)app->offset_bytes[1] << 16) |
                       ((uint32_t)app->offset_bytes[2] << 8) | (uint32_t)app->offset_bytes[3];

    esp32_flash_job_add_custom(app->flash_job, furi_string_get_cstr(app->pending_custom_path), offset);
    esp32_flash_job_save(app->flash_job, app->storage);

    // Collapse back through AddCustomFile straight to ManageFiles instead of
    // pushing a new instance, so repeated "add custom" runs don't grow the
    // scene stack unbounded.
    scene_manager_search_and_switch_to_previous_scene(
        app->scene_manager, Esp32C5FlasherSceneManageFiles);
}

void esp32_c5_flasher_scene_add_custom_offset_on_enter(void* context) {
    Esp32C5FlasherApp* app = context;

    memset(app->offset_bytes, 0, sizeof(app->offset_bytes));
    byte_input_set_header_text(app->byte_input, "Ziel-Offset (hex)");
    byte_input_set_result_callback(
        app->byte_input,
        add_custom_offset_result_callback,
        NULL,
        app,
        app->offset_bytes,
        sizeof(app->offset_bytes));

    view_dispatcher_switch_to_view(app->view_dispatcher, Esp32C5FlasherViewByteInput);
}

bool esp32_c5_flasher_scene_add_custom_offset_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void esp32_c5_flasher_scene_add_custom_offset_on_exit(void* context) {
    UNUSED(context);
}
