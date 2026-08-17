#define DODEV_PLUGIN_BUILD 1
#include "plugin/DoDevPluginAPI.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{
struct State
{
    const DoDevHostApi* host = nullptr;
    DoDevHandle inspector = nullptr;
};

std::string ReadText(State* state, DoDevHandle editor)
{
    if (!state || !state->host || !state->host->editor_get_text || !editor)
        return {};
    const size_t required = state->host->editor_get_text(state->host->context, editor, nullptr, 0);
    if (required == 0)
        return {};
    std::vector<char> buffer(required, 0);
    state->host->editor_get_text(state->host->context, editor, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

std::string ReadPath(State* state, DoDevHandle editor)
{
    if (!state || !state->host || !state->host->editor_get_path || !editor)
        return {};
    const size_t required = state->host->editor_get_path(state->host->context, editor, nullptr, 0);
    if (required == 0)
        return {};
    std::vector<char> buffer(required, 0);
    state->host->editor_get_path(state->host->context, editor, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

void InspectActiveEditor(void* userData)
{
    auto* state = static_cast<State*>(userData);
    if (!state || !state->host)
        return;
    DoDevHandle editor = state->host->active_editor(state->host->context);
    if (!editor)
    {
        state->host->set_status(state->host->context, "Example plugin: no active source editor");
        return;
    }

    std::string text = ReadText(state, editor);
    const std::string path = ReadPath(state, editor);
    if (text.size() > 4000)
        text.resize(4000);

    std::string report = "ACTIVE EDITOR\nPath: " + (path.empty() ? std::string("<untitled>") : path) +
                         "\nTabs: " + std::to_string(state->host->tab_count(state->host->context)) +
                         "\nCaret: " + std::to_string(state->host->editor_get_caret(state->host->context, editor)) +
                         "\n\n" + text;

    if (!state->inspector)
        state->inspector = state->host->create_text_panel(state->host->context,
                                                           DODEV_PANEL_LOCATION_SIDE,
                                                           "PLUGIN INSPECTOR",
                                                           report.c_str(), 1);
    else
        state->host->text_panel_set_text(state->host->context, state->inspector, report.c_str());
}

void NewScratchTab(void* userData)
{
    auto* state = static_cast<State*>(userData);
    if (!state || !state->host)
        return;
    state->host->new_editor_tab(state->host->context,
                                "Plugin Scratch",
                                "// Created by the example DoDevEditor plugin\n\n");
}
}

extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_load(const DoDevHostApi* host,
                                                       DoDevPluginInfo* info,
                                                       void** pluginState)
{
    if (!host || host->abi_version != DODEV_PLUGIN_ABI_VERSION || !info || !pluginState)
        return 0;

    auto state = std::make_unique<State>();
    state->host = host;

    info->abi_version = DODEV_PLUGIN_ABI_VERSION;
    info->size = sizeof(*info);
    info->id = "org.dodev.example.editor-access";
    info->name = "Editor Access Example";
    info->version = "1.0.0";
    info->description = "Demonstrates menus, active-editor access, tabs and host-created panels.";

    host->add_menu_item(host->context, "Plugins", "Example: Inspect Active Editor", "",
                        &InspectActiveEditor, state.get());
    host->add_menu_item(host->context, "Plugins", "Example: New Scratch Tab", "",
                        &NewScratchTab, state.get());
    host->log(host->context, DODEV_LOG_INFO, "Editor Access Example initialized");

    *pluginState = state.release();
    return 1;
}

extern "C" DODEV_PLUGIN_EXPORT void dodev_plugin_unload(void* pluginState)
{
    delete static_cast<State*>(pluginState);
}

extern "C" DODEV_PLUGIN_EXPORT void dodev_plugin_on_event(void* pluginState,
                                                            const DoDevEvent* event)
{
    auto* state = static_cast<State*>(pluginState);
    if (!state || !state->host || !event)
        return;
    if (event->type == DODEV_EVENT_ACTIVE_TAB_CHANGED)
        state->host->log(state->host->context, DODEV_LOG_DEBUG, "Example plugin observed active-tab change");
}
