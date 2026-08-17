#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#  ifdef DODEV_PLUGIN_BUILD
#    define DODEV_PLUGIN_EXPORT __declspec(dllexport)
#  else
#    define DODEV_PLUGIN_EXPORT
#  endif
#else
#  define DODEV_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DODEV_PLUGIN_ABI_VERSION 1u

typedef void* DoDevHandle;

typedef enum DoDevLogLevel
{
    DODEV_LOG_DEBUG = 0,
    DODEV_LOG_INFO = 1,
    DODEV_LOG_WARNING = 2,
    DODEV_LOG_ERROR = 3
} DoDevLogLevel;

typedef enum DoDevPanelId
{
    DODEV_PANEL_MAIN_FRAME = 0,
    DODEV_PANEL_FILE_TREE = 1,
    DODEV_PANEL_FOLDER_LIST = 2,
    DODEV_PANEL_SOURCE_CONTROL = 3,
    DODEV_PANEL_SYMBOLS = 4,
    DODEV_PANEL_SIDE_NOTEBOOK = 5,
    DODEV_PANEL_EDITOR_NOTEBOOK = 6,
    DODEV_PANEL_BOTTOM_NOTEBOOK = 7,
    DODEV_PANEL_AI_CHAT = 8,
    DODEV_PANEL_BOTTOM_LOGS = 9,
    DODEV_PANEL_BOTTOM_CONSOLE = 10,
    DODEV_PANEL_BOTTOM_ERRORS = 11,
    DODEV_PANEL_BOTTOM_OUTPUT = 12,
    DODEV_PANEL_BOTTOM_INSPECTOR = 13,
    DODEV_PANEL_SIDEBAR_ROOT = 14,
    DODEV_PANEL_EDITOR_ROOT = 15
} DoDevPanelId;

typedef enum DoDevPanelLocation
{
    DODEV_PANEL_LOCATION_SIDE = 1,
    DODEV_PANEL_LOCATION_BOTTOM = 2,
    DODEV_PANEL_LOCATION_EDITOR = 3
} DoDevPanelLocation;

typedef enum DoDevEventType
{
    DODEV_EVENT_ACTIVE_TAB_CHANGED = 1,
    DODEV_EVENT_EDITOR_OPENED = 2,
    DODEV_EVENT_EDITOR_CLOSED = 3,
    DODEV_EVENT_EDITOR_CHANGED = 4,
    DODEV_EVENT_WORKSPACE_CHANGED = 5
} DoDevEventType;

typedef struct DoDevEvent
{
    uint32_t size;
    DoDevEventType type;
    DoDevHandle object;
    int index;
    const char* path_utf8;
} DoDevEvent;

typedef void (*DoDevMenuCallback)(void* user_data);

/*
 * Advanced custom UI hook. The parent is a wxWindow* carried as an opaque
 * handle. A plugin that uses this factory must be compiled with an ABI-
 * compatible wxWidgets/toolchain and return a wxWindow*. Plugins that only use
 * the other host functions do not need to link wxWidgets at all.
 */
typedef DoDevHandle (*DoDevWindowFactory)(DoDevHandle parent, void* user_data);

typedef struct DoDevHostApi
{
    uint32_t abi_version;
    uint32_t size;
    void* context;

    void (*log)(void* context, DoDevLogLevel level, const char* message_utf8);
    DoDevHandle (*get_panel)(void* context, DoDevPanelId panel);
    int (*focus_panel)(void* context, DoDevPanelId panel);

    int (*tab_count)(void* context);
    int (*active_tab_index)(void* context);
    DoDevHandle (*tab_at)(void* context, int index);
    DoDevHandle (*active_tab)(void* context);
    int (*select_tab)(void* context, int index);
    int (*close_tab)(void* context, int index);
    size_t (*tab_title)(void* context, int index, char* out_utf8, size_t out_size);
    size_t (*tab_path)(void* context, int index, char* out_utf8, size_t out_size);

    DoDevHandle (*active_editor)(void* context);
    int (*is_editor)(void* context, DoDevHandle handle);
    size_t (*editor_get_text)(void* context, DoDevHandle editor, char* out_utf8, size_t out_size);
    int (*editor_set_text)(void* context, DoDevHandle editor, const char* text_utf8);
    int (*editor_insert_text)(void* context, DoDevHandle editor, size_t byte_position, const char* text_utf8);
    size_t (*editor_get_path)(void* context, DoDevHandle editor, char* out_utf8, size_t out_size);
    size_t (*editor_get_caret)(void* context, DoDevHandle editor);
    int (*editor_set_caret)(void* context, DoDevHandle editor, size_t byte_position);
    int (*editor_get_selection)(void* context, DoDevHandle editor, size_t* start, size_t* end);
    int (*editor_replace_selection)(void* context, DoDevHandle editor, const char* text_utf8);

    int (*open_file)(void* context, const char* path_utf8);
    DoDevHandle (*new_editor_tab)(void* context, const char* title_utf8, const char* initial_text_utf8);
    int (*set_status)(void* context, const char* text_utf8);

    int (*add_menu_item)(void* context,
                         const char* menu_utf8,
                         const char* label_utf8,
                         const char* shortcut_utf8,
                         DoDevMenuCallback callback,
                         void* user_data);
    int (*remove_menu_item)(void* context, int menu_token);

    /* Host-created text panel: no wxWidgets dependency is required by plugin. */
    DoDevHandle (*create_text_panel)(void* context,
                                     DoDevPanelLocation location,
                                     const char* title_utf8,
                                     const char* initial_text_utf8,
                                     int select);
    int (*text_panel_set_text)(void* context, DoDevHandle panel, const char* text_utf8);
    int (*remove_panel)(void* context, DoDevHandle panel);

    /* Advanced wxWidgets custom panel/tab support. */
    DoDevHandle (*add_custom_panel)(void* context,
                                    DoDevPanelLocation location,
                                    const char* title_utf8,
                                    DoDevWindowFactory factory,
                                    void* user_data,
                                    int select);
} DoDevHostApi;

typedef struct DoDevPluginInfo
{
    uint32_t abi_version;
    uint32_t size;
    const char* id;
    const char* name;
    const char* version;
    const char* description;
} DoDevPluginInfo;

typedef int (*DoDevPluginLoadFn)(const DoDevHostApi* host,
                                 DoDevPluginInfo* info,
                                 void** plugin_state);
typedef void (*DoDevPluginUnloadFn)(void* plugin_state);
typedef void (*DoDevPluginEventFn)(void* plugin_state, const DoDevEvent* event);

/* Required exported symbols in every plugin. */
DODEV_PLUGIN_EXPORT int dodev_plugin_load(const DoDevHostApi* host,
                                          DoDevPluginInfo* info,
                                          void** plugin_state);
DODEV_PLUGIN_EXPORT void dodev_plugin_unload(void* plugin_state);
/* Optional: dodev_plugin_on_event */

#ifdef __cplusplus
}
#endif
