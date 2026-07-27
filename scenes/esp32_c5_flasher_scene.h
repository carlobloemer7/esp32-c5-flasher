#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) Esp32C5FlasherScene##id,
typedef enum {
#include "esp32_c5_flasher_scene_config.h"
    Esp32C5FlasherSceneNum,
} Esp32C5FlasherScene;
#undef ADD_SCENE

extern const SceneManagerHandlers esp32_c5_flasher_scene_handlers;
