#include "flash_progress_view.h"

#include <gui/canvas.h>
#include <furi.h>
#include <stdlib.h>

typedef struct {
    FuriString* status_text;
    FuriString* file_name;
    uint8_t file_index;
    uint8_t total_files;
    uint8_t file_percent;
    uint8_t overall_percent;
    bool error;
    bool done;
} FlashProgressModel;

struct FlashProgressView {
    View* view;
};

static void flash_progress_view_draw_callback(Canvas* canvas, void* model_ptr) {
    FlashProgressModel* model = model_ptr;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "ESP32-C5 Flashen");

    canvas_set_font(canvas, FontSecondary);

    if(model->error) {
        canvas_draw_str(canvas, 2, 24, "Fehler:");
        canvas_draw_str(canvas, 2, 36, furi_string_get_cstr(model->status_text));
        canvas_draw_str(canvas, 2, 60, "Zurueck zum Beenden");
        return;
    }

    if(model->done) {
        canvas_draw_str(canvas, 2, 30, "Fertig!");
        canvas_draw_str(canvas, 2, 42, furi_string_get_cstr(model->status_text));
        canvas_draw_str(canvas, 2, 60, "Zurueck zum Beenden");
        return;
    }

    FuriString* line = furi_string_alloc();
    if(model->total_files > 0) {
        furi_string_printf(line, "Datei %u/%u:", model->file_index + 1, model->total_files);
        canvas_draw_str(canvas, 2, 22, furi_string_get_cstr(line));
    }
    canvas_draw_str(canvas, 2, 32, furi_string_get_cstr(model->file_name));

    // per-file progress bar
    canvas_draw_frame(canvas, 2, 38, 124, 8);
    uint8_t fw = (uint8_t)((model->file_percent * 122u) / 100u);
    if(fw > 0) canvas_draw_box(canvas, 3, 39, fw, 6);

    // overall progress bar
    canvas_draw_frame(canvas, 2, 50, 124, 8);
    uint8_t ow = (uint8_t)((model->overall_percent * 122u) / 100u);
    if(ow > 0) canvas_draw_box(canvas, 3, 51, ow, 6);

    furi_string_printf(line, "%u%%", model->overall_percent);
    canvas_draw_str(canvas, 108, 24, furi_string_get_cstr(line));

    canvas_draw_str(canvas, 2, 63, furi_string_get_cstr(model->status_text));

    furi_string_free(line);
}

FlashProgressView* flash_progress_view_alloc(void) {
    FlashProgressView* instance = malloc(sizeof(FlashProgressView));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(FlashProgressModel));
    view_set_draw_callback(instance->view, flash_progress_view_draw_callback);

    FlashProgressModel* model = view_get_model(instance->view);
    model->status_text = furi_string_alloc();
    model->file_name = furi_string_alloc();
    view_commit_model(instance->view, false);

    return instance;
}

void flash_progress_view_free(FlashProgressView* instance) {
    furi_check(instance);
    FlashProgressModel* model = view_get_model(instance->view);
    furi_string_free(model->status_text);
    furi_string_free(model->file_name);
    view_commit_model(instance->view, false);

    view_free(instance->view);
    free(instance);
}

View* flash_progress_view_get_view(FlashProgressView* instance) {
    furi_check(instance);
    return instance->view;
}

void flash_progress_view_reset(FlashProgressView* instance, uint8_t total_files) {
    FlashProgressModel* model = view_get_model(instance->view);
    furi_string_set(model->status_text, "Starte...");
    furi_string_reset(model->file_name);
    model->file_index = 0;
    model->total_files = total_files;
    model->file_percent = 0;
    model->overall_percent = 0;
    model->error = false;
    model->done = false;
    view_commit_model(instance->view, true);
}

void flash_progress_view_set_status(FlashProgressView* instance, const char* text) {
    FlashProgressModel* model = view_get_model(instance->view);
    furi_string_set(model->status_text, text);
    view_commit_model(instance->view, true);
}

void flash_progress_view_set_current_file(
    FlashProgressView* instance,
    const char* file_name,
    uint8_t file_index) {
    FlashProgressModel* model = view_get_model(instance->view);
    furi_string_set(model->file_name, file_name);
    model->file_index = file_index;
    model->file_percent = 0;
    view_commit_model(instance->view, true);
}

void flash_progress_view_set_progress(
    FlashProgressView* instance,
    uint8_t file_percent,
    uint8_t overall_percent) {
    FlashProgressModel* model = view_get_model(instance->view);
    model->file_percent = file_percent;
    model->overall_percent = overall_percent;
    view_commit_model(instance->view, true);
}

void flash_progress_view_set_error(FlashProgressView* instance, const char* text) {
    FlashProgressModel* model = view_get_model(instance->view);
    furi_string_set(model->status_text, text);
    model->error = true;
    view_commit_model(instance->view, true);
}

void flash_progress_view_set_done(FlashProgressView* instance) {
    FlashProgressModel* model = view_get_model(instance->view);
    furi_string_set(model->status_text, "Alle Dateien geschrieben.");
    model->done = true;
    model->overall_percent = 100;
    view_commit_model(instance->view, true);
}
