#include "../esp32_c5_flasher_i.h"
#include "../esp32/esp32_paths.h"
#include "../esp32/esp32_file_copy.h"

// This scene has no view of its own: it opens the (blocking, full-screen) SD
// card file browser directly from on_enter, then returns to ManageFiles -
// same pattern the firmware's own BadUSB app uses for its file-select scene.
// The browser starts at the category folder but is NOT restricted to it, so
// picking an existing file there works, and picking a file from anywhere
// else on the card also works (it gets copied into the category folder).
void esp32_c5_flasher_scene_file_category_on_enter(void* context) {
    Esp32C5FlasherApp* app = context;

    const char* folder = esp32_flash_job_category_folder(app->active_category);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ESP32_FILE_EXTENSION, NULL);
    browser_options.base_path = STORAGE_EXT_PATH_PREFIX;

    furi_string_set(app->file_path, folder);
    bool picked =
        dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options);

    if(picked) {
        FuriString* dest_path = furi_string_alloc();
        if(esp32_file_copy_into_folder(
               app->storage, furi_string_get_cstr(app->file_path), folder, dest_path)) {
            esp32_flash_job_set_category_path(
                app->flash_job, app->active_category, furi_string_get_cstr(dest_path));
            esp32_flash_job_save(app->flash_job, app->storage);
        }
        furi_string_free(dest_path);
    }

    scene_manager_previous_scene(app->scene_manager);
}

bool esp32_c5_flasher_scene_file_category_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void esp32_c5_flasher_scene_file_category_on_exit(void* context) {
    UNUSED(context);
}
