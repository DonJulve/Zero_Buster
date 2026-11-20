#pragma once

#include "zero_buster.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Save Custom HID configuration to MicroSD */
void save_custom_hid_config(Storage* storage, const FuriHalUsbHidConfig* cfg);

/* Load or generate default Custom HID config from MicroSD */
void load_custom_hid_config(Storage* storage, FuriHalUsbHidConfig* cfg);

/* Ensure required directories exist on MicroSD under apps_data */
void ensure_storage_dirs(Storage* storage);

/* Scan /ext/apps_data/zero_buster/payloads for available files */
void scan_payloads(ZeroBusterApp* app);

/* Scan /ext/badusb/assets/layouts for available .kl files */
void scan_layouts(ZeroBusterApp* app);

/* Load a .kl layout file or fallback to built-in en-US hid_asciimap */
bool load_layout(ZeroBusterApp* app, const char* layout_name);

/* Generate next incremental loot filepath in /ext/apps_data/zero_buster/loot/ */
void get_next_loot_filepath(Storage* storage, char* out_path, size_t max_len);

#ifdef __cplusplus
}
#endif
