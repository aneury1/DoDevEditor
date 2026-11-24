#pragma once
#include "EditorData.h"
#include "TerminalSession.h"
#include "TerminalScreen.h"
#include "dodev/settings/SettingsManager.h"
#include "dodev/themes/ThemeManager.h"
#include "dodev/templates/TemplateRegistry.h"
#include "dodev/plugins/PluginManager.h"
#include "dodev/editor/EditorCommands.h"
#include "dodev/terminal/TerminalTasks.h"
#include <Raven.h>
#include <filesystem>
#include <memory>
#include <map>
#include <string>
#include <vector>

namespace dodev {
class EditorApp;
struct TerminalTab {
    EditorApp* app=nullptr;
    TerminalSession session;
    TerminalScreen screen;
    Rid canvas=R_INVALID_ID;
    Rid group=R_INVALID_ID;
    std::string title;
    float char_width=8.0f, line_height=18.0f;
    bool dragging=false,has_selection=false;
    int anchor_row=0,anchor_col=0,selection_row=0,selection_col=0;
};
enum class DocumentKind { Text, Csv, Dlt, Binary, DataServer };
struct Document {
    EditorApp* app=nullptr;
    fs::path path;
    std::string title;
    DocumentKind kind=DocumentKind::Text;
    Rid page=R_INVALID_ID;
    Rid editor=R_INVALID_ID;
    Rid table=R_INVALID_ID;
    Rid filter=R_INVALID_ID;
    bool dirty=false;
    bool read_only=false;
    std::vector<DltEntry> dlt_entries;
};
class EditorApp {
public:
    int run(int argc, char** argv);
    void opened(const fs::path& file);
    void changed(Rid editor);
    void filterDlt(Document& doc);
private:
    SettingsManager settings_;
    TemplateRegistry templateRegistry_;
    ThemeManager themeRegistry_;
    PluginManager pluginManager_{settings_,templateRegistry_,themeRegistry_};
    Rid window_=R_INVALID_ID;
    Rid settingsWindow_=R_INVALID_ID;
    Rid designerWindow_=R_INVALID_ID;
    Rid templateWindow_=R_INVALID_ID;
    Rid editorFont_=R_INVALID_ID,uiFont_=R_INVALID_ID,terminalFont_=R_INVALID_ID;
    std::vector<std::string> templateIds_;
    std::vector<std::string> pluginCandidates_;
    std::map<std::string,Rid> pluginTabs_,pluginPanels_;
    std::vector<std::string> themeNames_;

    Rid tabs_=R_INVALID_ID;
    Rid dock_=R_INVALID_ID;
    Rid explorer_=R_INVALID_ID;
    Rid git_pane_=R_INVALID_ID;
    Rid output_pane_=R_INVALID_ID;
    Rid status_=R_INVALID_ID;
    Rid terminal_tabs_=R_INVALID_ID;
    Rid terminal_state_=R_INVALID_ID;
    Rid terminal_profile_=R_INVALID_ID;
    Rid active_terminal_canvas_=R_INVALID_ID;
    std::vector<terminal::Task> tasks_;
    std::vector<std::unique_ptr<TerminalTab>> terminals_;
    int next_terminal_id_=0;
    Rid browser_=R_INVALID_ID;
    Rid git_list_=R_INVALID_ID;
    Rid git_detail_=R_INVALID_ID;
    Rid cursor_=R_INVALID_ID;
    Rid themes_=R_INVALID_ID;
    fs::path folder_;
    std::string theme_="Music Nocturne";
    std::vector<std::unique_ptr<Document>> docs_;
    std::vector<GitCommit> commits_;
    int untitled_count_=0;
    void initUi();
    void bindActions();
    void handleAction(const std::string& name);
    Document* active();
    Document* findDocument(Rid id);
    Document& addDocument(DocumentKind kind, const std::string& title, const fs::path& path={});
    void newDocument();
    void csvDocument(const fs::path& file, const std::string& content);
    void dltDocument(const fs::path& file, const std::vector<std::uint8_t>& content);
    void dataServerDocument();
    void refreshCsv(Document& doc);
    void refreshGit();
    void gitSelected(int index);
    void openDialog();
    void openFolderDialog();
    bool save(Document& doc, bool save_as=false);
    bool allowClose(Document& doc);
    bool allowExit();
    void closeActive();
    void setFolder(const fs::path& folder);
    void setStatus(const std::string& message);
    void log(const std::string& message);
    void updateCursor();
    TerminalTab* activeTerminal();
    void newTerminal(bool split=false);
    void refreshTasks();
    bool executeTask(const terminal::Task& task);
    bool dispatchTerminalMenu(const std::string& action);
    bool dispatchEditSelection(const std::string& action);
    void findWorkspace();
    void closeTerminal();
    void pumpTerminals();
    void copyTerminal();
    void pasteTerminal();
    void focusTerminal();
    void clearTerminal();
    static void terminalPaintCb(Rid,RPainter*,float,float,void*);
    static void terminalMeasureCb(Rid,float*,float*,void*);
    static int terminalEventCb(Rid,const REvent*,void*);
    static void terminalTabCb(Rid,int,void*);
    static int terminalTabCloseCb(Rid,int,Rid,void*);
    void showSearch(bool show);
    void searchNext(bool replace=false, bool all=false, bool backwards=false);
    void applyTheme(int index);
    void refreshThemeDecor();
    // Settings, templates and plugin UI are created lazily from embedded XML.
    void openSettings();
    void fillSettings();
    void applySettings(bool restoreDefaults=false);
    void browseSetting(const std::string& action);
    void populatePluginSettings();
    void updatePluginSettingChoice();
    void pluginAction(const std::string& action);
    void openTemplateWindow();
    void templatePreview();
    void createTemplateDocument();
    void openThemeDesigner();
    ThemeDefinition designerDefinition()const;
    void themeDesignerAction(const std::string& action);
    void loadUserThemes();
    void refreshThemeChoices();
    void registerPluginUi();
    void openPluginView(const std::string& id);
    void applyEditorOptions();
    void applyFonts();
    void loadPrefs();
    void savePrefs();
    fs::path prefsFile() const;
    static void pluginListCb(Rid,int,void*);
    static void pluginKeyCb(Rid,int,void*);
    static void templateSelectCb(Rid,int,void*);
    static void templateFilenameCb(Rid,const char*,void*);
    static void pluginUiLabel(void*,const char*);
    static void pluginUiButton(void*,const char*,const char*);
    static void pluginUiText(void*,const char*);
    static void actionCb(Rid sender,const char* action,void* user_data);
    static void browserCb(Rid browser,const char* path,void* user_data);
    static void textCb(Rid editor,void* user_data);
    static void themeCb(Rid,int,void*);
    static void tabCb(Rid,int,void*);
    static int tabCloseCb(Rid,int,Rid,void*);
    static int windowCloseCb(Rid,void*);
    static void gitCb(Rid,int,void*);
    static void timerCb(Rid,void*);
    static void dltFilterCb(Rid,const char*,void*);
    static void dltRefreshCb(Rid,void*);
};
} // namespace dodev
