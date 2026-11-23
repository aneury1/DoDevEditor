#pragma once
#include "EditorData.h"
#include "RemoteJournal.h"
#include "SerialMonitor.h"
#include "SourceSymbols.h"
#include "TerminalScreen.h"
#include "TerminalSession.h"
#include "dodev/editor/EditorCommands.h"
#include "dodev/keybindings/KeybindingManager.h"
#include "dodev/plugins/PluginManager.h"
#include "dodev/settings/SettingsManager.h"
#include "dodev/templates/TemplateRegistry.h"
#include "dodev/terminal/TerminalTasks.h"
#include "dodev/themes/ThemeManager.h"
#include <Raven.h>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dodev {
class EditorApp;
struct TerminalTab {
  EditorApp *app = nullptr;
  TerminalSession session;
  TerminalScreen screen;
  Rid canvas = R_INVALID_ID;
  Rid group = R_INVALID_ID;
  std::string title;
  float char_width = 8.0f, line_height = 18.0f;
  bool dragging = false, has_selection = false;
  int anchor_row = 0, anchor_col = 0, selection_row = 0, selection_col = 0;
};
enum class DocumentKind { Text, Csv, Dlt, Binary, DataServer };
struct Document {
  EditorApp *app = nullptr;
  fs::path path;
  std::string title;
  DocumentKind kind = DocumentKind::Text;
  Rid page = R_INVALID_ID;
  Rid editor = R_INVALID_ID;
  Rid table = R_INVALID_ID;
  Rid filter = R_INVALID_ID;
  bool dirty = false;
  bool read_only = false;
  std::vector<DltEntry> dlt_entries;
};
class EditorApp {
public:
  int run(int argc, char **argv);
  void opened(const fs::path &file);
  void changed(Rid editor);
  void filterDlt(Document &doc);

private:
  SettingsManager settings_;
  keys::KeybindingManager keybindings_{settings_};
  TemplateRegistry templateRegistry_;
  ThemeManager themeRegistry_;
  PluginManager pluginManager_{settings_, templateRegistry_, themeRegistry_};
  Rid window_ = R_INVALID_ID;
  Rid settingsWindow_ = R_INVALID_ID;
  Rid designerWindow_ = R_INVALID_ID;
  Rid templateWindow_ = R_INVALID_ID;
  Rid pluginPromptWindow_ = R_INVALID_ID;
  Rid symbolsTaskPage_ = R_INVALID_ID;
  Rid symbolsList_ = R_INVALID_ID;
  Rid symbolsFilter_ = R_INVALID_ID;
  Rid symbolsDescription_ = R_INVALID_ID;
  Rid symbolsStatus_ = R_INVALID_ID;
  Rid symbolsEditor_ = R_INVALID_ID;
  std::vector<symbols::Symbol> symbolsAll_, symbolsVisible_;
  Rid journalDialog_ = R_INVALID_ID;
  Rid journalTaskPage_ = R_INVALID_ID, journalTaskLog_ = R_INVALID_ID;
  Rid journalTaskFilter_ = R_INVALID_ID, journalTaskMode_ = R_INVALID_ID;
  Rid journalTaskStatus_ = R_INVALID_ID;
  journal::Session journalCapture_;
  std::string journalLastText_, journalLastStatus_;
  bool journalOnlyMatches_ = true;
  Rid serialWindow_ = R_INVALID_ID;
  Rid serialTaskPage_ = R_INVALID_ID;
  Rid serialTaskLog_ = R_INVALID_ID;
  Rid serialTaskStatus_ = R_INVALID_ID;
  Rid serialTaskFilterInput_ = R_INVALID_ID;
  Rid serialTxInput_ = R_INVALID_ID;
  Rid serialRawCanvas_ = R_INVALID_ID;
  Rid serialTaskFilterMode_ = R_INVALID_ID;
  serial::CaptureSession serialCapture_;
  std::string serialLastText_, serialLastStatus_;
  bool serialOnlyMatches_ = true;
  std::function<void(const std::string &)> pendingPluginPrompt_;
  Rid goToLineWindow_ = R_INVALID_ID, quickOpenWindow_ = R_INVALID_ID;
  bool commandPalette_ = false, recordingBinding_ = false;
  std::vector<fs::path> quickCandidates_, quickMatches_;
  std::vector<std::string> quickCommands_, keybindingRows_;
  std::map<std::string, std::string> originalMenuLabels_;
  std::vector<keys::Shortcut> installedShortcuts_;
  Rid editorFont_ = R_INVALID_ID, uiFont_ = R_INVALID_ID,
      terminalFont_ = R_INVALID_ID;
  std::vector<std::string> templateIds_;
  std::vector<std::string> pluginCandidates_;
  std::map<std::string, Rid> pluginTabs_, pluginPanels_;
  std::vector<std::string> themeNames_;

  Rid tabs_ = R_INVALID_ID;
  Rid dock_ = R_INVALID_ID;
  Rid explorer_ = R_INVALID_ID;
  Rid git_pane_ = R_INVALID_ID;
  Rid output_pane_ = R_INVALID_ID;
  Rid status_ = R_INVALID_ID;
  Rid terminal_tabs_ = R_INVALID_ID;
  Rid terminal_state_ = R_INVALID_ID;
  Rid terminal_profile_ = R_INVALID_ID;
  Rid active_terminal_canvas_ = R_INVALID_ID;
  std::vector<terminal::Task> tasks_;
  std::vector<std::unique_ptr<TerminalTab>> terminals_;
  int next_terminal_id_ = 0;
  Rid browser_ = R_INVALID_ID;
  Rid git_list_ = R_INVALID_ID;
  Rid git_detail_ = R_INVALID_ID;
  Rid cursor_ = R_INVALID_ID;
  Rid themes_ = R_INVALID_ID;
  fs::path folder_;
  std::string theme_ = "Music Nocturne";
  std::vector<std::unique_ptr<Document>> docs_;
  std::vector<GitCommit> commits_;
  int untitled_count_ = 0;
  void initUi();
  void bindActions();
  void handleAction(const std::string &name);
  Document *active();
  Document *findDocument(Rid id);
  Document &addDocument(DocumentKind kind, const std::string &title,
                        const fs::path &path = {});
  void newDocument();
  void csvDocument(const fs::path &file, const std::string &content);
  void dltDocument(const fs::path &file,
                   const std::vector<std::uint8_t> &content);
  void dataServerDocument();
  void refreshCsv(Document &doc);
  void refreshGit();
  void gitSelected(int index);
  void openDialog();
  void openFolderDialog();
  bool save(Document &doc, bool save_as = false);
  bool allowClose(Document &doc);
  bool allowExit();
  void closeActive();
  void setFolder(const fs::path &folder);
  void setStatus(const std::string &message);
  void log(const std::string &message);
  void updateCursor();
  TerminalTab *activeTerminal();
  void newTerminal(bool split = false);
  void refreshTasks();
  bool executeTask(const terminal::Task &task);
  bool dispatchTerminalMenu(const std::string &action);
  bool dispatchEditSelection(const std::string &action);
  void findWorkspace();
  void closeTerminal();
  void pumpTerminals();
  void copyTerminal();
  void exportTerminal();
  void exportAllTerminals();
  void pasteTerminal();
  void focusTerminal();
  void clearTerminal();
  static void terminalPaintCb(Rid, RPainter *, float, float, void *);
  static void terminalMeasureCb(Rid, float *, float *, void *);
  static int terminalEventCb(Rid, const REvent *, void *);
  static void terminalTabCb(Rid, int, void *);
  static int terminalTabCloseCb(Rid, int, Rid, void *);
  void showSearch(bool show);
  void searchNext(bool replace = false, bool all = false,
                  bool backwards = false);
  void applyTheme(int index);
  void refreshThemeDecor();
  // Settings, templates and plugin UI are created lazily from embedded XML.
  void installKeybindings();
  void refreshShortcutLabels();
  void openGoToLine();
  void goToLine();
  void openQuickOpen(bool commands = false);
  void refreshQuickOpen();
  void chooseQuickOpen();
  void refreshKeybindingRows();
  void chooseKeybinding();
  void keybindingAction(const std::string &action);
  static void quickFilterCb(Rid, const char *, void *);
  static void quickSelectCb(Rid, int, void *);
  static void quickCommitCb(Rid, const char *, void *);
  static void goToLineCommitCb(Rid, const char *, void *);
  static int keybindingEventCb(Rid, const REvent *, void *);
  static void keybindingFilterCb(Rid, const char *, void *);
  static void keybindingSelectCb(Rid, int, void *);
  void openSettings();
  void fillSettings();
  void applySettings(bool restoreDefaults = false);
  void browseSetting(const std::string &action);
  void populatePluginSettings();
  void updatePluginSettingChoice();
  void pluginAction(const std::string &action);
  void openTemplateWindow();
  void templatePreview();
  void createTemplateDocument();
  void openThemeDesigner();
  ThemeDefinition designerDefinition() const;
  void themeDesignerAction(const std::string &action);
  void loadUserThemes();
  void refreshThemeChoices();
  void registerPluginUi();
  bool showPluginPrompt(const std::string &title, const std::string &label,
                        const std::string &initial,
                        std::function<void(const std::string &)> completion);
  void closePluginPrompt(bool accept);
  void installPluginHooks();
  bool openSerialMonitor();
  void openSerialTasks();
  void refreshSerialTasks();
  void serialAction(const std::string &action);
  bool serialSend(const std::string &bytes);
  static void serialTxCommitCb(Rid, const char *, void *);
  static int serialConsoleEventCb(Rid, const REvent *, void *);
  static void serialConsolePaintCb(Rid, RPainter *, float, float, void *);
  static void serialConsoleMeasureCb(Rid, float *, float *, void *);
  bool serialApplyFilter();
  bool openSymbolsPanel();
  void refreshSymbolsPanel();
  void filterSymbolsPanel();
  void symbolsAction(const std::string &action);
  static void symbolsFilterCb(Rid, const char *, void *);
  static void symbolsSelectCb(Rid, int, void *);
  static void symbolsActivatedCb(Rid, int, void *);
  bool openRemoteJournal();
  void openJournalTasks();
  void refreshJournalTasks();
  void journalAction(const std::string &action);
  static void journalFilterCommitCb(Rid, const char *, void *);
  void refreshSerialPortChoices();
  static void serialPortSelected(Rid, int, void *);
  void openPluginView(const std::string &id);
  void applyEditorOptions();
  void applyFonts();
  void loadPrefs();
  void savePrefs();
  fs::path prefsFile() const;
  static void pluginListCb(Rid, int, void *);
  static void pluginKeyCb(Rid, int, void *);
  static void templateSelectCb(Rid, int, void *);
  static void templateFilenameCb(Rid, const char *, void *);
  static void pluginUiLabel(void *, const char *);
  static void pluginUiButton(void *, const char *, const char *);
  static void pluginUiText(void *, const char *);
  static void actionCb(Rid sender, const char *action, void *user_data);
  static void browserCb(Rid browser, const char *path, void *user_data);
  static void textCb(Rid editor, void *user_data);
  static void themeCb(Rid, int, void *);
  static void tabCb(Rid, int, void *);
  static int tabCloseCb(Rid, int, Rid, void *);
  static int windowCloseCb(Rid, void *);
  static void gitCb(Rid, int, void *);
  static void timerCb(Rid, void *);
  static void dltFilterCb(Rid, const char *, void *);
  static void dltRefreshCb(Rid, void *);
};
} // namespace dodev
