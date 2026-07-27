#include "../esp32_c5_flasher_i.h"
#include "../esp32/esp32_rom_protocol.h"

#include <furi/core/thread.h>
#include <string.h>

static const char* basename_of(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void flashing_progress_callback(const Esp32FlashProgressEvent* event, void* context) {
    Esp32C5FlasherApp* app = context;
    switch(event->type) {
    case Esp32FlashProgressStartFile:
        flash_progress_view_set_current_file(
            app->flash_progress_view, basename_of(event->file_name), event->file_index);
        break;
    case Esp32FlashProgressFileProgress:
        flash_progress_view_set_progress(
            app->flash_progress_view, event->file_percent, event->overall_percent);
        break;
    case Esp32FlashProgressFileDone:
        break;
    }
}

static int32_t flashing_worker_thread(void* context) {
    Esp32C5FlasherApp* app = context;
    Esp32FlashJob* job = app->flash_job;

    flash_progress_view_reset(
        app->flash_progress_view, (uint8_t)esp32_flash_job_resolved_count(job));
    flash_progress_view_set_status(
        app->flash_progress_view,
        job->auto_reset_enabled ? "Verbinde (Auto-Reset)..." :
                                   "BOOT halten + RESET druecken...");

    Esp32RomProtocol* protocol = esp32_rom_protocol_alloc(app->storage);

    Esp32FlashResult result = esp32_rom_protocol_connect(
        protocol,
        job->baud_rate,
        job->auto_reset_enabled,
        job->gpio27_assist_enabled,
        &app->flash_abort_requested);

    if(result == Esp32FlashResultOk) {
        flash_progress_view_set_status(app->flash_progress_view, "Verbunden, flashe...");
        result = esp32_rom_protocol_flash_job(
            protocol, job, flashing_progress_callback, app, &app->flash_abort_requested);
    }

    esp32_rom_protocol_disconnect(protocol);
    esp32_rom_protocol_free(protocol);

    if(app->flash_abort_requested) {
        flash_progress_view_set_error(app->flash_progress_view, "Abgebrochen.");
    } else if(result == Esp32FlashResultOk) {
        flash_progress_view_set_done(app->flash_progress_view);
    } else {
        const char* msg = "Unbekannter Fehler";
        switch(result) {
        case Esp32FlashResultSyncTimeout:
            msg = "Kein SYNC - Verkabelung/Bootloader-Modus pruefen";
            break;
        case Esp32FlashResultProtocolError:
            msg = "Protokollfehler beim Schreiben";
            break;
        case Esp32FlashResultFileError:
            msg = "Datei konnte nicht gelesen werden";
            break;
        default:
            break;
        }
        flash_progress_view_set_error(app->flash_progress_view, msg);
    }

    return 0;
}

void esp32_c5_flasher_scene_flashing_on_enter(void* context) {
    Esp32C5FlasherApp* app = context;

    app->flash_abort_requested = false;
    view_dispatcher_switch_to_view(app->view_dispatcher, Esp32C5FlasherViewFlashProgress);

    app->flash_thread = furi_thread_alloc();
    furi_thread_set_name(app->flash_thread, "Esp32FlashWorker");
    furi_thread_set_stack_size(app->flash_thread, 3 * 1024);
    furi_thread_set_context(app->flash_thread, app);
    furi_thread_set_callback(app->flash_thread, flashing_worker_thread);
    furi_thread_start(app->flash_thread);
}

bool esp32_c5_flasher_scene_flashing_on_event(void* context, SceneManagerEvent event) {
    Esp32C5FlasherApp* app = context;
    if(event.type != SceneManagerEventTypeBack) return false;

    app->flash_abort_requested = true;
    if(app->flash_thread) {
        furi_thread_join(app->flash_thread);
        furi_thread_free(app->flash_thread);
        app->flash_thread = NULL;
    }

    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, Esp32C5FlasherSceneMainMenu);
    return true;
}

void esp32_c5_flasher_scene_flashing_on_exit(void* context) {
    Esp32C5FlasherApp* app = context;
    if(app->flash_thread) {
        app->flash_abort_requested = true;
        furi_thread_join(app->flash_thread);
        furi_thread_free(app->flash_thread);
        app->flash_thread = NULL;
    }
}
