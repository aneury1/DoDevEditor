#pragma once
#include "dodev_plugin_api.h"
namespace dodev::sdk {
inline int setting(const DodevHostAPI* h, const char* key, const char* def,
                   const char* desc, const char* schema = "{}") {
  return DODEV_HOST_HAS(h, register_setting_ex)
             ? h->register_setting_ex(h->context, key, def, desc, schema)
             : h->register_setting(h->context, key, def, desc);
}
inline int settingsAction(const DodevHostAPI* h, const char* title,
                          const char* command) {
  return !DODEV_HOST_HAS(h, register_settings_action) ||
         h->register_settings_action(h->context, title, command);
}
}  // namespace dodev::sdk
