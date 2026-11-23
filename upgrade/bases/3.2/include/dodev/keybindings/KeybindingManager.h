#pragma once
#include "dodev/settings/SettingsManager.h"
#include <map>
#include <string>
#include <vector>
namespace dodev::keys {
struct Binding { std::string command; std::string key; bool customized=false; };
// Pure C++ shortcut syntax, independent of Raven/OS key codes.
struct Shortcut { std::string key; unsigned modifiers=0; bool valid=false; };
// Modifier flags map to CTRL, SHIFT, ALT, SUPER in that order.
constexpr unsigned Ctrl=1, Shift=2, Alt=4, Super=8;
Shortcut parseShortcut(const std::string& source);
std::string formatShortcut(const Shortcut& key);
std::string normalizeShortcut(const std::string& text);
class KeybindingManager {
public:
    explicit KeybindingManager(SettingsManager& settings);
    void load();
    bool save(std::string* error=nullptr)const;
    // Empty key explicitly unbinds a command. Conflicts are rejected.
    bool set(const std::string& command,const std::string& key,std::string* error=nullptr);
    void reset(const std::string& command);
    void resetAll();
    const std::map<std::string,std::string>& bindings()const{return bindings_;}
    std::vector<Binding> all()const;
    std::string get(const std::string& command)const;
    const std::vector<std::string>& diagnostics()const{return diagnostics_;}
    bool maySave()const{return !malformed_;}
    bool hasCommand(const std::string& command)const;
    void registerCommand(const std::string& command,const std::string& defaultKey="");
    static std::map<std::string,std::string> defaults();
private:
    SettingsManager& settings_;
    std::map<std::string,std::string> defaults_;
    std::map<std::string,std::string> bindings_;
    std::map<std::string,std::string> dormantOverrides_;
    bool malformed_=false;
    std::vector<std::string> diagnostics_;
};
} // namespace dodev::keys
