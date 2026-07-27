#include "../esp32_c5_flasher_i.h"

#define BAUD_OPTIONS_COUNT 4
static const uint32_t baud_options[BAUD_OPTIONS_COUNT] = {115200, 230400, 460800, 921600};
static const char* const baud_labels[BAUD_OPTIONS_COUNT] = {"115200", "230400", "460800", "921600"};

static void auto_reset_changed(VariableItem* item) {
    Esp32C5FlasherApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->flash_job->auto_reset_enabled = index != 0;
    variable_item_set_current_value_text(item, app->flash_job->auto_reset_enabled ? "An" : "Aus");
    esp32_flash_job_save(app->flash_job, app->storage);
}

static void gpio27_assist_changed(VariableItem* item) {
    Esp32C5FlasherApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->flash_job->gpio27_assist_enabled = index != 0;
    variable_item_set_current_value_text(item, app->flash_job->gpio27_assist_enabled ? "An" : "Aus");
    esp32_flash_job_save(app->flash_job, app->storage);
}

static void baud_changed(VariableItem* item) {
    Esp32C5FlasherApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->flash_job->baud_rate = baud_options[index];
    variable_item_set_current_value_text(item, baud_labels[index]);
    esp32_flash_job_save(app->flash_job, app->storage);
}

static uint8_t baud_index_for(uint32_t baud) {
    for(uint8_t i = 0; i < BAUD_OPTIONS_COUNT; i++) {
        if(baud_options[i] == baud) return i;
    }
    return 0;
}

void esp32_c5_flasher_scene_settings_on_enter(void* context) {
    Esp32C5FlasherApp* app = context;
    variable_item_list_reset(app->variable_item_list);

    VariableItem* item;

    item = variable_item_list_add(app->variable_item_list, "Auto-Reset", 2, auto_reset_changed, app);
    variable_item_set_current_value_index(item, app->flash_job->auto_reset_enabled ? 1 : 0);
    variable_item_set_current_value_text(item, app->flash_job->auto_reset_enabled ? "An" : "Aus");

    item = variable_item_list_add(
        app->variable_item_list, "Baudrate", BAUD_OPTIONS_COUNT, baud_changed, app);
    uint8_t baud_idx = baud_index_for(app->flash_job->baud_rate);
    variable_item_set_current_value_index(item, baud_idx);
    variable_item_set_current_value_text(item, baud_labels[baud_idx]);

    item = variable_item_list_add(
        app->variable_item_list, "GPIO27-Assist", 2, gpio27_assist_changed, app);
    variable_item_set_current_value_index(item, app->flash_job->gpio27_assist_enabled ? 1 : 0);
    variable_item_set_current_value_text(
        item, app->flash_job->gpio27_assist_enabled ? "An" : "Aus");

    view_dispatcher_switch_to_view(app->view_dispatcher, Esp32C5FlasherViewVariableItemList);
}

bool esp32_c5_flasher_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void esp32_c5_flasher_scene_settings_on_exit(void* context) {
    Esp32C5FlasherApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
