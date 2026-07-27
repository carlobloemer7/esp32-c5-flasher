#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FlashProgressView FlashProgressView;

FlashProgressView* flash_progress_view_alloc(void);
void flash_progress_view_free(FlashProgressView* instance);
View* flash_progress_view_get_view(FlashProgressView* instance);

// Resets the view to its initial "starting" state - call once before a new
// flash job begins.
void flash_progress_view_reset(FlashProgressView* instance, uint8_t total_files);

void flash_progress_view_set_status(FlashProgressView* instance, const char* text);
void flash_progress_view_set_current_file(
    FlashProgressView* instance,
    const char* file_name,
    uint8_t file_index);
// file_percent/overall_percent are 0-100.
void flash_progress_view_set_progress(
    FlashProgressView* instance,
    uint8_t file_percent,
    uint8_t overall_percent);
void flash_progress_view_set_error(FlashProgressView* instance, const char* text);
void flash_progress_view_set_done(FlashProgressView* instance);

#ifdef __cplusplus
}
#endif
