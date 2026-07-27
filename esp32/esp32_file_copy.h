#pragma once

#include <furi.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

// Copies src_path into dest_folder, keeping its basename. If src_path is
// already inside dest_folder, this is a no-op (just returns that path).
// Writes the resulting path into out_dest_path. Returns false on I/O error.
bool esp32_file_copy_into_folder(
    Storage* storage,
    const char* src_path,
    const char* dest_folder,
    FuriString* out_dest_path);

#ifdef __cplusplus
}
#endif
