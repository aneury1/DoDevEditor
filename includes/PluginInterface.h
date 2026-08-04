#pragma once
#include <string>

class PluginLoader
{
public:
    void* Load(const std::string& path);

    void* GetSymbol(void* handle, const std::string& name);

    void Unload(void* handle);
};

/*
/// Sample>
expose this interface
extern "C"
{
    typedef void (*RegisterFn)(CommandRegistry*);
}

extern "C"
void RegisterPlugin(CommandRegistry* registry)
{
    registry->Register("file.new", "New File", []()
    {
        wxLogMessage("New File executed from plugin");
    });

    registry->Register("git.commit", "Git Commit", []()
    {
        wxLogMessage("Commit from plugin");
    });
}

// Load at runtime
PluginLoader loader;

void* handle = loader.Load("gitplugin.so");

auto fn = (void(*)(CommandRegistry*)) loader.GetSymbol(
    handle,
    "RegisterPlugin");

if (fn)
    fn(&commands);
*/