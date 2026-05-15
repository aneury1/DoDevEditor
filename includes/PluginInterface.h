#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <string>

class PluginLoader
{
public:
    void* Load(const std::string& path)
    {
#ifdef _WIN32
        return (void*)LoadLibraryA(path.c_str());
#else
        return dlopen(path.c_str(), RTLD_LAZY);
#endif
    }

    void* GetSymbol(void* handle, const std::string& name)
    {
#ifdef _WIN32
        return (void*)GetProcAddress((HMODULE)handle, name.c_str());
#else
        return dlsym(handle, name.c_str());
#endif
    }

    void Unload(void* handle)
    {
#ifdef _WIN32
        FreeLibrary((HMODULE)handle);
#else
        dlclose(handle);
#endif
    }
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