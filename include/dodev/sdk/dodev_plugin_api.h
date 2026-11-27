#ifndef DODEV_PLUGIN_API_H
#define DODEV_PLUGIN_API_H
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#define DODEV_PLUGIN_EXPORT __declspec(dllexport)
#else
#define DODEV_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define DODEV_PLUGIN_API_VERSION 1u

typedef struct DodevHostAPI DodevHostAPI;
typedef struct DodevUiBuilder DodevUiBuilder;
typedef void (*DodevCommandFn)(const DodevHostAPI* host, void* user_data);
typedef void (*DodevEventFn)(const DodevHostAPI* host, const char* event,
                             const char* json_payload, void* user_data);
typedef void (*DodevViewFn)(const DodevHostAPI* host, const DodevUiBuilder* ui,
                            void* user_data);
/* Input callbacks run later on the Raven UI thread.  The plugin must remain
 * loaded. */
typedef void (*DodevRowFn)(const DodevHostAPI* host, const char* row_id,
                           void* user_data);
typedef void (*DodevPromptFn)(const DodevHostAPI* host, const char* text,
                              void* user_data);

/* Generic form submission and cancellable worker. All form strings are borrowed
   during the callback; a worker must never invoke UI or host APIs. */
typedef struct DodevFormField {
  const char* label;
  const char* initial;
  int multiline;
} DodevFormField;
typedef void (*DodevFormFn)(const DodevHostAPI*, const char* const*, size_t,
                            void*);
typedef int (*DodevCancelledFn)(void*);
typedef void (*DodevWorkFn)(void*, DodevCancelledFn, void*);

/* The builder is short lived. Plugins must not store it or its context. */
struct DodevUiBuilder {
  uint32_t struct_size;
  void* context;
  void (*label)(void* context, const char* text);
  void (*button)(void* context, const char* title, const char* command_id);
  void (*read_only_text)(void* context, const char* text);
  /* Row-major strings and IDs are copied synchronously; callback remains valid
   * while loaded. */
  void (*table)(void* context, const char* const* columns, size_t column_count,
                const char* const* cells, const char* const* row_ids,
                size_t row_count, const DodevHostAPI* host, DodevRowFn selected,
                void* user_data);
  /* Optional inline workspace profiles editor. Host copies the default JSON.
     Uses the registered view owner and current workspace, not a global setting.
   */
  void (*workspace_profiles)(void* context, const char* default_json);
  /* Host copies fields. Callback executes on the UI thread on submit. */
  void (*form)(void*, const DodevFormField*, size_t, const char*,
               const DodevHostAPI*, DodevFormFn, void*);
};
struct DodevHostAPI {
  uint32_t struct_size;
  uint32_t api_version;
  void* context;
  void (*log)(void* context, int severity, const char* message);
  int (*register_command)(void* context, const char* id, const char* title,
                          DodevCommandFn callback, void* user_data);
  /* Options is supported by the wx host since 5.2.3. Menu commands may be
     registered command IDs or host actions plugin.view.<registered-view-id>. */
  int (*register_menu)(void* context, const char* location, const char* title,
                       const char* command_id);
  /* Editor tab. register_panel with side Bottom targets the Output notebook;
     Tab targets the editor; Left/Right target the workspace sidebar in wx. */
  int (*register_tab)(void* context, const char* id, const char* title,
                      DodevViewFn factory, void* user_data);
  int (*register_panel)(void* context, const char* id, const char* title,
                        const char* side, DodevViewFn factory, void* user_data);
  int (*register_template)(void* context, const char* id, const char* name,
                           const char* extension, const char* contents);
  int (*register_theme_xml)(void* context, const char* theme_xml);
  int (*register_setting)(void* context, const char* key,
                          const char* default_value, const char* description);
  const char* (*get_setting)(void* context, const char* key);
  int (*set_setting)(void* context, const char* key, const char* value);
  int (*subscribe_event)(void* context, const char* event,
                         DodevEventFn callback, void* user_data);
  /* Optional additive ABI v1 extension. Check struct_size before dereferencing.
   */
  int (*get_active_document)(void* context, const char** path,
                             const char** contents);
  int (*replace_active_document)(void* context, const char* contents);
  int (*execute_terminal_command)(void* context, const char* line);
  int (*prompt_input)(void* context, const char* title, const char* label,
                      const char* initial, DodevPromptFn callback,
                      void* user_data);
  /* Optional additive extension: opens the host-managed serial monitor tool
   * window. */
  int (*open_serial_monitor)(void* context);
  int (*open_source_symbols)(void* context);
  int (*open_remote_journal)(void* context);
  /* Optional additive ABI v1 hook introduced in Raven 3.7. */
  int (*open_ai_assistant)(void* context);
  /* GDB extension (4.1): returned paths live until the next call. Line is
   * 1-based. */
  int (*debug_context)(void* context, const char** workspace, const char** file,
                       int* line);
  int (*debug_launch)(void* context, const char* cwd, const char* const* argv,
                      size_t argc);
  /* Returns 1 accepted, 0 no session, -1 busy/rejected. @state, @pause, @stop
   * are host operations. */
  int (*debug_control)(void* context, const char* command);
  int (*debug_open_file)(void* context, const char* path);
  void (*debug_status)(void* context, const char* message);
  /* Additive v1 settings schema. Values remain strings for ABI compatibility.
     Schema JSON: label, type (text/multiline/bool/int/choice), min/max,
     choices (strings or {value,label}), optional legacyKey for migration. */
  int (*register_setting_ex)(void* context, const char* key,
                             const char* default_value, const char* description,
                             const char* schema_json);
  int (*register_settings_action)(void* context, const char* title,
                                  const char* command_id);
  /* Workspace configuration belongs to the calling plugin. Host opens its JSON
   * file. */
  int (*open_workspace_config)(void* context, const char* initial_json);
  /* Secret and path callbacks run synchronously before these functions return.
   */
  int (*prompt_secret)(void* context, const char* title, const char* label,
                       DodevPromptFn callback, void* user_data);
  /* Display or replace a named read-only tool view; side: Editor, Output,
   * Workspace. */
  int (*show_text_view)(void* context, const char* id, const char* title,
                        const char* side, const char* text);
  int (*select_path)(void* context, const char* title, int directory,
                     DodevPromptFn callback, void* user_data);
  /* Runs worker off the UI thread; waits with cancellation UI. No plugin code
     remains running when this call returns. Worker may only use cancel
     callback. */
  int (*run_job)(void*, const char*, DodevWorkFn, void*);
};
/* Test optional host function availability before use; older binaries may not
 * expose it. */
#define DODEV_HOST_HAS(host, field)                                         \
  ((host) &&                                                                \
   (host)->struct_size >=                                                   \
       (uint32_t)(offsetof(DodevHostAPI, field) + sizeof((host)->field)) && \
   (host)->field)
/* Returned plugin function table: plugin may omit shutdown by leaving NULL. */
typedef struct DodevPluginAPI {
  uint32_t struct_size;
  uint32_t api_version;
  void (*shutdown)(void);
} DodevPluginAPI;
/* Required exported symbol of every native plugin. Return 1 for success. */
typedef int (*DodevPluginInitFn)(const DodevHostAPI*, DodevPluginAPI*);
#ifdef __cplusplus
}
#endif
#endif
