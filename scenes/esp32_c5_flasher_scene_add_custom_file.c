#include "../esp32_c5_flasher_i.h"
#include "../esp32/esp32_paths.h"
#include "../esp32/esp32_file_copy.h"

// No view of its own - opens the SD browser directly, copies the picked
// file into custom/, then hands off to AddCustomOffset for the offset entry.
void esp32_c5_flasher_scene_add_custom_file_on_enter(void* context) {
    Esp32C5FlasherApp* app = context;

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ESP32_FILE_EXTENSION, NULL);
    browser_options.base_path = STORAGE_EXT_PATH_PREFIX;

    furi_string_set(app->file_path, STORAGE_EXT_PATH_PREFIX);
    bool picked =
        dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options);

    if(!picked) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(!esp32_file_copy_into_folder(
           app->storage, furi_string_get_cstr(app->file_path), ESP32_CUSTOM_FOLDER, app->pending_custom_path)) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    scene_manager_next_scene(app->scene_manager, Esp32C5FlasherSceneAddCustomOffset);
}

bool esp32_c5_flasher_scene_add_custom_file_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void esp32_c5_flasher_scene_add_custom_file_on_exit(void* context) {
    UNUSED(context);
}
