#include "esp32_c5_flasher_scene.h"

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void* context);
#include "esp32_c5_flasher_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "esp32_c5_flasher_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "esp32_c5_flasher_scene_config.h"
#undef ADD_SCENE

void (*const esp32_c5_flasher_on_enter_handlers[])(void*) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
#include "esp32_c5_flasher_scene_config.h"
#undef ADD_SCENE
};

bool (*const esp32_c5_flasher_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
#include "esp32_c5_flasher_scene_config.h"
#undef ADD_SCENE
};

void (*const esp32_c5_flasher_on_exit_handlers[])(void*) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
#include "esp32_c5_flasher_scene_config.h"
#undef ADD_SCENE
};

const SceneManagerHandlers esp32_c5_flasher_scene_handlers = {
    .on_enter_handlers = esp32_c5_flasher_on_enter_handlers,
    .on_event_handlers = esp32_c5_flasher_on_event_handlers,
    .on_exit_handlers = esp32_c5_flasher_on_exit_handlers,
    .scene_num = Esp32C5FlasherSceneNum,
};
