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
typedef void (*DodevCommandFn)(const DodevHostAPI* host,void* user_data);
typedef void (*DodevEventFn)(const DodevHostAPI* host,const char* event,const char* json_payload,void* user_data);
typedef void (*DodevViewFn)(const DodevHostAPI* host,const DodevUiBuilder* ui,void* user_data);
/* Input callbacks run later on the Raven UI thread.  The plugin must remain loaded. */
typedef void (*DodevPromptFn)(const DodevHostAPI* host,const char* text,void* user_data);

/* The builder is short lived. Plugins must not store it or its context. */
struct DodevUiBuilder {
    uint32_t struct_size;
    void* context;
    void (*label)(void* context,const char* text);
    void (*button)(void* context,const char* title,const char* command_id);
    void (*read_only_text)(void* context,const char* text);
};
struct DodevHostAPI {
    uint32_t struct_size;
    uint32_t api_version;
    void* context;
    void (*log)(void* context,int severity,const char* message);
    int (*register_command)(void* context,const char* id,const char* title,DodevCommandFn callback,void* user_data);
    int (*register_menu)(void* context,const char* location,const char* title,const char* command_id);
    int (*register_tab)(void* context,const char* id,const char* title,DodevViewFn factory,void* user_data);
    int (*register_panel)(void* context,const char* id,const char* title,const char* side,DodevViewFn factory,void* user_data);
    int (*register_template)(void* context,const char* id,const char* name,const char* extension,const char* contents);
    int (*register_theme_xml)(void* context,const char* theme_xml);
    int (*register_setting)(void* context,const char* key,const char* default_value,const char* description);
    const char* (*get_setting)(void* context,const char* key);
    int (*set_setting)(void* context,const char* key,const char* value);
    int (*subscribe_event)(void* context,const char* event,DodevEventFn callback,void* user_data);
    /* Optional additive ABI v1 extension. Check struct_size before dereferencing. */
    int (*get_active_document)(void* context,const char** path,const char** contents);
    int (*replace_active_document)(void* context,const char* contents);
    int (*execute_terminal_command)(void* context,const char* line);
    int (*prompt_input)(void* context,const char* title,const char* label,
                        const char* initial,DodevPromptFn callback,void* user_data);
    /* Optional additive extension: opens the host-managed serial monitor tool window. */
    int (*open_serial_monitor)(void* context);
    int (*open_source_symbols)(void* context);
    int (*open_remote_journal)(void* context);
    /* Optional additive ABI v1 hook introduced in Raven 3.7. */
    int (*open_ai_assistant)(void* context);
    /* GDB extension (4.1): returned paths live until the next call. Line is 1-based. */
    int (*debug_context)(void* context,const char** workspace,const char** file,int* line);
    int (*debug_launch)(void* context,const char* cwd,const char* const* argv,size_t argc);
    /* Returns 1 accepted, 0 no session, -1 busy/rejected. @state, @pause, @stop are host operations. */
    int (*debug_control)(void* context,const char* command);
    int (*debug_open_file)(void* context,const char* path);
    void (*debug_status)(void* context,const char* message);
};
/* Test optional host function availability before use; older binaries may not expose it. */
#define DODEV_HOST_HAS(host, field) ((host) && (host)->struct_size >= \
    (uint32_t)(offsetof(DodevHostAPI,field)+sizeof((host)->field)) && (host)->field)
/* Returned plugin function table: plugin may omit shutdown by leaving NULL. */
typedef struct DodevPluginAPI {
    uint32_t struct_size;
    uint32_t api_version;
    void (*shutdown)(void);
} DodevPluginAPI;
/* Required exported symbol of every native plugin. Return 1 for success. */
typedef int (*DodevPluginInitFn)(const DodevHostAPI*,DodevPluginAPI*);
#ifdef __cplusplus
}
#endif
#endif
