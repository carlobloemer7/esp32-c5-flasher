#include "esp32_c5_flasher_i.h"

#include <stdlib.h>
#include <string.h>

static bool esp32_c5_flasher_custom_event_callback(void* context, uint32_t event) {
    Esp32C5FlasherApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool esp32_c5_flasher_navigation_event_callback(void* context) {
    Esp32C5FlasherApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static Esp32C5FlasherApp* esp32_c5_flasher_app_alloc(void) {
    Esp32C5FlasherApp* app = malloc(sizeof(Esp32C5FlasherApp));
    memset(app, 0, sizeof(Esp32C5FlasherApp));

    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->flash_job = esp32_flash_job_alloc();
    esp32_flash_job_ensure_folders(app->storage);
    esp32_flash_job_load(app->flash_job, app->storage);

    app->file_path = furi_string_alloc();
    app->pending_custom_path = furi_string_alloc();

    app->scene_manager = scene_manager_alloc(&esp32_c5_flasher_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, esp32_c5_flasher_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, esp32_c5_flasher_navigation_event_callback);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Esp32C5FlasherViewSubmenu, submenu_get_view(app->submenu));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        Esp32C5FlasherViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Esp32C5FlasherViewByteInput, byte_input_get_view(app->byte_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Esp32C5FlasherViewWidget, widget_get_view(app->widget));

    app->flash_progress_view = flash_progress_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        Esp32C5FlasherViewFlashProgress,
        flash_progress_view_get_view(app->flash_progress_view));

    return app;
}

static void esp32_c5_flasher_app_free(Esp32C5FlasherApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, Esp32C5FlasherViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, Esp32C5FlasherViewVariableItemList);
    variable_item_list_free(app->variable_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, Esp32C5FlasherViewByteInput);
    byte_input_free(app->byte_input);

    view_dispatcher_remove_view(app->view_dispatcher, Esp32C5FlasherViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, Esp32C5FlasherViewFlashProgress);
    flash_progress_view_free(app->flash_progress_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);

    esp32_flash_job_free(app->flash_job);
    furi_string_free(app->file_path);
    furi_string_free(app->pending_custom_path);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);

    free(app);
}

int32_t esp32_c5_flasher_app(void* p) {
    UNUSED(p);
    Esp32C5FlasherApp* app = esp32_c5_flasher_app_alloc();

    scene_manager_next_scene(app->scene_manager, Esp32C5FlasherSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    esp32_c5_flasher_app_free(app);
    return 0;
}
