#include "zero_buster_storage.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Save Custom HID configuration to MicroSD in apps_data */
void save_custom_hid_config(Storage* storage, const FuriHalUsbHidConfig* cfg) {
    ensure_storage_dirs(storage);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, CUSTOM_HID_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[256];
        snprintf(
            buf,
            sizeof(buf),
            "# Zero Buster Custom HID Config\n"
            "VID=0x%04lX\n"
            "PID=0x%04lX\n"
            "MANUF=%s\n"
            "PRODUCT=%s\n",
            (unsigned long)cfg->vid,
            (unsigned long)cfg->pid,
            cfg->manuf[0] ? cfg->manuf : "Custom Corp",
            cfg->product[0] ? cfg->product : "Custom Keyboard");
        storage_file_write(f, buf, strlen(buf));
        storage_file_close(f);
    }
    storage_file_free(f);
}

/* Load or generate default Custom HID config from MicroSD */
void load_custom_hid_config(Storage* storage, FuriHalUsbHidConfig* cfg) {
    ensure_storage_dirs(storage);
    File* f = storage_file_alloc(storage);

    // Baseline fallback values
    cfg->vid = 0x046D;
    cfg->pid = 0xC31C;
    strncpy(cfg->manuf, "Logitech", sizeof(cfg->manuf) - 1);
    cfg->manuf[sizeof(cfg->manuf) - 1] = '\0';
    strncpy(cfg->product, "Keyboard K120", sizeof(cfg->product) - 1);
    cfg->product[sizeof(cfg->product) - 1] = '\0';

    if(storage_file_open(f, CUSTOM_HID_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[256];
        uint16_t bytes_read = storage_file_read(f, buf, sizeof(buf) - 1);
        if(bytes_read > 0) {
            buf[bytes_read] = '\0';
            char* p = buf;
            while(*p) {
                while(*p == '\r' || *p == '\n' || *p == ' ') p++;
                if(!*p) break;
                if(*p == '#') {
                    while(*p && *p != '\n') p++;
                    continue;
                }
                char* end = p;
                while(*end && *end != '\r' && *end != '\n') end++;
                char next_char = *end;
                *end = '\0';

                if(strncmp(p, "VID=", 4) == 0) {
                    cfg->vid = (uint16_t)strtoul(p + 4, NULL, 0);
                } else if(strncmp(p, "PID=", 4) == 0) {
                    cfg->pid = (uint16_t)strtoul(p + 4, NULL, 0);
                } else if(strncmp(p, "MANUF=", 6) == 0) {
                    char* val = p + 6;
                    if(strlen(val) > 0) {
                        strncpy(cfg->manuf, val, sizeof(cfg->manuf) - 1);
                        cfg->manuf[sizeof(cfg->manuf) - 1] = '\0';
                    }
                } else if(strncmp(p, "PRODUCT=", 8) == 0) {
                    char* val = p + 8;
                    if(strlen(val) > 0) {
                        strncpy(cfg->product, val, sizeof(cfg->product) - 1);
                        cfg->product[sizeof(cfg->product) - 1] = '\0';
                    }
                }

                if(next_char == '\0') break;
                p = end + 1;
            }
        }
        storage_file_close(f);
    }
    storage_file_free(f);
}

/* Ensure required directories exist on MicroSD under apps_data */
void ensure_storage_dirs(Storage* storage) {
    storage_simply_mkdir(storage, APP_DATA_DIR);
    storage_simply_mkdir(storage, PAYLOAD_DIR);
    storage_simply_mkdir(storage, LOOT_DIR);
}

/* Scan /ext/apps_data/zero_buster/payloads for available files */
void scan_payloads(ZeroBusterApp* app) {
    ensure_storage_dirs(app->storage);

    app->payloads.count = 0;
    app->payloads.selected_index = 0;

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, PAYLOAD_DIR)) {
        FileInfo info;
        char name[MAX_PAYLOAD_NAME_LEN];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(name[0] == '.') continue;
            if(!file_info_is_dir(&info)) {
                strncpy(
                    app->payloads.names[app->payloads.count],
                    name,
                    MAX_PAYLOAD_NAME_LEN - 1);
                app->payloads.names[app->payloads.count][MAX_PAYLOAD_NAME_LEN - 1] = '\0';
                app->payloads.count++;
                if(app->payloads.count >= MAX_PAYLOADS) break;
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
}

/* Scan /ext/badusb/assets/layouts for available .kl files */
void scan_layouts(ZeroBusterApp* app) {
    app->layouts.count = 0;
    app->layouts.selected_index = 0;

    // US English is always the first entry and default
    strncpy(app->layouts.names[0], "en-US", MAX_LAYOUT_NAME_LEN - 1);
    app->layouts.names[0][MAX_LAYOUT_NAME_LEN - 1] = '\0';
    app->layouts.count = 1;

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, BADUSB_LAYOUTS_DIR)) {
        FileInfo info;
        char name[64];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(name[0] == '.') continue;
            if(!file_info_is_dir(&info)) {
                size_t len = strlen(name);
                if(len > 3 && strcmp(&name[len - 3], ".kl") == 0) {
                    name[len - 3] = '\0';
                    // Skip en-US since it's already added at index 0
                    if(strcmp(name, "en-US") == 0) continue;

                    if(app->layouts.count < MAX_LAYOUTS) {
                        strncpy(
                            app->layouts.names[app->layouts.count],
                            name,
                            MAX_LAYOUT_NAME_LEN - 1);
                        app->layouts.names[app->layouts.count][MAX_LAYOUT_NAME_LEN - 1] = '\0';
                        app->layouts.count++;
                    }
                }
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);

    // Load default layout (en-US)
    load_layout(app, app->layouts.names[app->layouts.selected_index]);
}

/* Load a .kl layout file or fallback to built-in en-US hid_asciimap */
bool load_layout(ZeroBusterApp* app, const char* layout_name) {
    // Always start with built-in en-US as fallback
    memcpy(app->active_layout, hid_asciimap, sizeof(app->active_layout));

    if(!layout_name || !layout_name[0] || strcmp(layout_name, "en-US") == 0) {
        return true;
    }

    char path[128];
    snprintf(path, sizeof(path), "%s/%s.kl", BADUSB_LAYOUTS_DIR, layout_name);

    File* file = storage_file_alloc(app->storage);
    bool loaded = false;
    uint8_t bytes[128 * sizeof(uint16_t)];

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_size(file) == sizeof(bytes) &&
           storage_file_read(file, bytes, sizeof(bytes)) == sizeof(bytes)) {
            for(size_t i = 0; i < 128; i++) {
                app->active_layout[i] = bytes[2 * i] | ((uint16_t)bytes[2 * i + 1] << 8);
            }
            loaded = true;
        }
        storage_file_close(file);
    }
    storage_file_free(file);

    if(!loaded) {
        FURI_LOG_W(TAG, "Failed to load .kl layout '%s', using en-US", layout_name);
    }
    return loaded;
}

/* Helper to generate incremental loot filenames in apps_data */
void get_next_loot_filepath(Storage* storage, char* out_path, size_t max_len) {
    ensure_storage_dirs(storage);
    uint32_t index = 1;
    while(index < 1000) {
        snprintf(out_path, max_len, "%s/loot_%03lu.txt", LOOT_DIR, (unsigned long)index);
        FileInfo info;
        if(storage_common_stat(storage, out_path, &info) != FSE_OK) {
            break;
        }
        index++;
    }
}
