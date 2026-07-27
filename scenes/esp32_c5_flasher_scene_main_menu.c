#include "../esp32_c5_flasher_i.h"

typedef enum {
    MainMenuIndexFlash,
    MainMenuIndexManageFiles,
    MainMenuIndexSettings,
} MainMenuIndex;

static void main_menu_callback(void* context, uint32_t index) {
    Esp32C5FlasherApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void esp32_c5_flasher_scene_main_menu_on_enter(void* context) {
    Esp32C5FlasherApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ESP32-C5 Flasher");
    submenu_add_item(app->submenu, "Flashen starten", MainMenuIndexFlash, main_menu_callback, app);
    submenu_add_item(
        app->submenu, "Firmware verwalten", MainMenuIndexManageFiles, main_menu_callback, app);
    submenu_add_item(app->submenu, "Einstellungen", MainMenuIndexSettings, main_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, Esp32C5FlasherViewSubmenu);
}

bool esp32_c5_flasher_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    Esp32C5FlasherApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case MainMenuIndexFlash:
        scene_manager_next_scene(app->scene_manager, Esp32C5FlasherSceneFirmwareSelect);
        return true;
    case MainMenuIndexManageFiles:
        scene_manager_next_scene(app->scene_manager, Esp32C5FlasherSceneManageFiles);
        return true;
    case MainMenuIndexSettings:
        scene_manager_next_scene(app->scene_manager, Esp32C5FlasherSceneSettings);
        return true;
    default:
        return false;
    }
}

void esp32_c5_flasher_scene_main_menu_on_exit(void* context) {
    Esp32C5FlasherApp* app = context;
    submenu_reset(app->submenu);
}
