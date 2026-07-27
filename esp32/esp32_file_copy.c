#include "esp32_file_copy.h"

#include <string.h>

bool esp32_file_copy_into_folder(
    Storage* storage,
    const char* src_path,
    const char* dest_folder,
    FuriString* out_dest_path) {
    const char* slash = strrchr(src_path, '/');
    const char* base_name = slash ? slash + 1 : src_path;

    furi_string_printf(out_dest_path, "%s/%s", dest_folder, base_name);

    if(furi_string_cmp_str(out_dest_path, src_path) == 0) {
        // Already in the destination folder, nothing to do.
        return true;
    }

    File* src = storage_file_alloc(storage);
    File* dst = storage_file_alloc(storage);

    bool ok = storage_file_open(src, src_path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(ok) {
        ok = storage_file_open(
            dst, furi_string_get_cstr(out_dest_path), FSAM_WRITE, FSOM_CREATE_ALWAYS);
    }

    if(ok) {
        uint8_t buf[512];
        size_t got;
        while((got = storage_file_read(src, buf, sizeof(buf))) > 0) {
            if(storage_file_write(dst, buf, got) != got) {
                ok = false;
                break;
            }
        }
    }

    storage_file_close(src);
    storage_file_close(dst);
    storage_file_free(src);
    storage_file_free(dst);

    return ok;
}
