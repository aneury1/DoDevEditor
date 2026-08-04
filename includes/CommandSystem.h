#pragma once
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>

#include "CommandSystem.h"
struct MenuItemDef
{
    std::string label;
    std::string commandId;
    std::string shortcut;
    bool isSeparator = false;
};

struct MenuDef
{
    std::string name;
    std::vector<MenuItemDef> items;
};

class CommandRegistry
{
public:
 
    CommandRegistry() = default;
    CommandRegistry(const CommandRegistry& oth) = default;
 
    using Callback = std::function<void()>;

    void Register(const std::string& id, Callback cb);

    void Execute(const std::string& id);
    bool Exists(const std::string& id) const;

    std::vector<MenuDef> GetTitles();

private:
    std::unordered_map<std::string, Callback> commands;
};


class MenuRegistry
{
public:
    void AddMenu(const std::string& name);

    void AddItem(const std::string& menu,
                 const MenuItemDef& item);

    const std::unordered_map<std::string, std::vector<MenuItemDef>>& GetMenus() const;

private:
    std::unordered_map<std::string, std::vector<MenuItemDef>> menus;
};


class IPlugin
{
public:
    virtual ~IPlugin() = default;

    virtual void RegisterCommands(CommandRegistry& commands) = 0;
    virtual void RegisterMenus(MenuRegistry& menus) = 0;
};

/*
EXAMPLE:

class GitPlugin : public IPlugin
{
public:
    void RegisterCommands(CommandRegistry& commands) override
    {
        commands.Register("git.commit", []()
        {
            wxLogMessage("Git Commit triggered");
        });

        commands.Register("git.push", []()
        {
            wxLogMessage("Git Push triggered");
        });
    }

    void RegisterMenus(MenuRegistry& menus) override
    {
        menus.AddMenu("Git");

        menus.AddItem("Git", {
            "Commit",
            "git.commit",
            "Ctrl+G C"
        });

        menus.AddItem("Git", {
            "Push",
            "git.push",
            "Ctrl+G P"
        });
    }
};


/// TODO: class DynamicMenuBar : public wxMenuBar
{
public:
    DynamicMenuBar(CommandRegistry& cmd, MenuRegistry& reg)
        : commands(cmd), registry(reg)
    {
        Build();
    }

    void Rebuild()
    {
        ClearMenus();
        Build();
    }

private:
    void Build()
    {
        for (auto& [menuName, items] : registry.GetMenus())
        {
            wxMenu* menu = new wxMenu();

            for (const auto& item : items)
            {
                if (item.isSeparator)
                {
                    menu->AppendSeparator();
                    continue;
                }

                wxString label = item.label;
                if (!item.shortcut.empty())
                    label += "\t" + item.shortcut;

                int id = wxWindow::NewControlId();

                menu->Append(id, label);

                Bind(wxEVT_MENU, [this, cmdId = item.commandId](wxCommandEvent&)
                {
                    commands.Execute(cmdId);
                }, id);
            }

            Append(menu, menuName);
        }
    }

    void ClearMenus()
    {
        for (size_t i = 0; i < GetMenuCount(); ++i)
            Remove(0);
    }

private:
    CommandRegistry& commands;
    MenuRegistry& registry;
};

CommandRegistry commands;
MenuRegistry menus;

// Load plugins
GitPlugin git;
git.RegisterCommands(commands);
git.RegisterMenus(menus);

// Build menu
auto* mb = new DynamicMenuBar(commands, menus);
SetMenuBar(mb);

*/