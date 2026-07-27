#include "../esp32_c5_flasher_i.h"

#include <string.h>

static const char* basename_of(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void manage_files_callback(void* context, uint32_t index) {
    Esp32C5FlasherApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void esp32_c5_flasher_scene_manage_files_on_enter(void* context) {
    Esp32C5FlasherApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Firmware verwalten");

    FuriString* label = furi_string_alloc();
    for(size_t i = 0; i < Esp32FlashCategoryCount; i++) {
        Esp32FlashEntry* entry = &app->flash_job->categories[i];
        const char* filename = furi_string_empty(entry->path) ? "(keine Datei)" :
                                                                  basename_of(furi_string_get_cstr(entry->path));
        furi_string_printf(label, "%s: %s", esp32_flash_job_category_name((Esp32FlashCategory)i), filename);
        submenu_add_item(app->submenu, furi_string_get_cstr(label), i, manage_files_callback, app);
    }
    furi_string_free(label);

    submenu_add_item(
        app->submenu, "Eigene Binaries hinzufuegen", Esp32FlashCategoryCount, manage_files_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, Esp32C5FlasherViewSubmenu);
}

bool esp32_c5_flasher_scene_manage_files_on_event(void* context, SceneManagerEvent event) {
    Esp32C5FlasherApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event < Esp32FlashCategoryCount) {
        app->active_category = (Esp32FlashCategory)event.event;
        scene_manager_next_scene(app->scene_manager, Esp32C5FlasherSceneFileCategory);
        return true;
    } else if(event.event == Esp32FlashCategoryCount) {
        scene_manager_next_scene(app->scene_manager, Esp32C5FlasherSceneAddCustomFile);
        return true;
    }
    return false;
}

void esp32_c5_flasher_scene_manage_files_on_exit(void* context) {
    Esp32C5FlasherApp* app = context;
    submenu_reset(app->submenu);
}
