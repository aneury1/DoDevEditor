
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>

#include "CommandSystem.h"

void CommandRegistry::Register(const std::string &id, Callback cb)
{
    commands[id] = cb;
}

void CommandRegistry::Execute(const std::string &id)
{
    if (commands.count(id))
        commands[id]();
}

bool CommandRegistry::Exists(const std::string &id) const
{
    return commands.count(id);
}

std::vector<MenuDef> CommandRegistry::GetTitles()
{
    std::vector<MenuDef> ret;
    for (auto it : commands)
    {
        MenuDef def;
        def.name = it.first;
        ret.emplace_back(def);
    }
    return ret;
}

void MenuRegistry::AddMenu(const std::string &name)
{
    menus[name]; // create if not exists
}

void MenuRegistry::AddItem(const std::string &menu,
             const MenuItemDef &item)
{
    menus[menu].push_back(item);
}

const std::unordered_map<std::string, std::vector<MenuItemDef>> &MenuRegistry::GetMenus() const
{
    return menus;
}

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