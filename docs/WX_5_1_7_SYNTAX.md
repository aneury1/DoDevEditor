# DoDevEditor wxWidgets 5.1.7 — Syntax Highlighting

Open **Settings → Syntax Highlighting**. The existing 13 themes remain the base palettes. This tab adds non-destructive overrides; nothing is changed until Apply or Save & Close is pressed. Preview edits and Reset operate on the settings draft, and Cancel discards changes not yet applied.

## Editing styles

1. Choose `All themes (general)` or a specific installed theme.
2. Choose `All languages (general)` or a specific language.
3. Choose a token category (keyword, comment, string, number, operator, identifier, type, function, preprocessor, property, error, tag or default). Not every lexer emits every category.
4. For an individual keyword, choose `Exact keyword (language-specific)` and pick a keyword from the language's list. The same spelling in strings and comments is not recolored as a keyword.
5. Check Override beside a foreground color, background color or font size. Font weight and italic, underline and strikethrough support an explicit Inherit choice. Preview rule updates the example; most controls update the preview immediately. Use Apply to update open documents.
6. Reset rule removes the selected override; Reset all syntax removes all syntax overrides in the draft. Restore defaults resets the syntax draft and other settings.

The preview uses the selected theme; if the scope is All themes it uses the theme selected on the Appearance page. The font baseline is the editor font size at dialog creation.

## Languages

C, C++, Java, Kotlin, C#, JavaScript, TypeScript, Go, Python, JSON/JSONC, Shell/Bash, SQL, HTML, XML, CSS, YAML, CMake, Makefile, Lua, Rust, Markdown, and plain text. C-family languages share Scintilla's C++ lexer but use separate keyword dictionaries; Java/Kotlin/JS/TS/Go/C# are approximate syntax modes, not semantic parsers. Shell extensions for zsh, JSX/TSX and CSS variants are approximate as well. `.h` defaults to C++; `.C` is also C++. SQL and CMake keyword matching is case-insensitive. Exact keyword options are offered where a keyword dictionary exists; markup, CSS, Markdown and Makefile use category-based customization. No semantic highlighting, project-aware symbol classification, or arbitrary user-provided lexer definition is included.

## Persistence

Uses the existing SettingsManager, usually `~/.config/DoDevEditor/settings.json` on Linux (or `DODEV_CONFIG_DIR` when set). No theme XML or JSON files are changed. Example property in the settings JSON:

```json
"syntaxHighlighting": {
  "global": {
    "*": {"tokens": {"comment": {"italic": true}}},
    "cpp": {"tokens": {"keyword": {"fontWeight": 700}}}
  },
  "themes": {
    "VS Code Dark Modern": {
      "cpp": {
        "tokens": {"string": {"foreground": "#CE9178"}},
        "keywords": {"return": {"foreground": "#FFAA66", "background": "#303030", "fontSize": 16, "fontWeight": 700, "strike": true}}
      }
    }
  }
}
```

Merge this property into the existing settings object rather than replacing the entire file. Precedence, per property: base theme -> global all-language category -> global language category -> theme all-language category -> theme language category -> global exact keyword -> theme exact keyword. An explicit false overrides inherited true. Color values are #RRGGBB; sizes are 8–64 points; weights are 100–900. Invalid properties are ignored. Theme-specific rules use the exact theme name, including custom themes. Theme import/export does not export these independent syntax overrides.

## Rendering

Category-only configurations use the native lexer. Exact keyword overrides use an offscreen native lexer and container styling for the visible document, preserving the editor text and undo buffer. Strikethrough is drawn by a Scintilla indicator. Changes are refreshed after a 200 ms idle debounce by the editor timer. Styling is limited to 96 distinct exact keyword overrides per language/theme (reserved style IDs 160–255); excess entries are retained in settings but not rendered. Files over 1 MiB use category styling only (no exact keyword overlays or strike indicators), to bound GUI-thread work. Native styling is restored automatically when exact rules are cleared. Fold state may reset on transition between container and native lexer. No terminal syntax changes.

## Testing

Headless Debug tests:

```sh
cmake -S . -B build-headless -DDODEV_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-headless --target dodev_syntax_tests dodev_settings_tests dodev_keybindings_tests -j4
ctest --test-dir build-headless -R 'dodev_syntax_tests|settings_themes_templates_json|vscode_keybinding_settings|wx_raven_menu_parity' --output-on-failure
```

All four passed in the packaging environment. The complete inherited suite was not rerun. wxWidgets development files are unavailable, so the native implementation and GUI regression test have not been compiled or run. On a system with wxWidgets 3.2 and a display (or Xvfb):

```sh
cmake -S . -B build -DDODEV_WX_SYNTAX_GUI_TEST=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target DoDevEditor dodev_wx_syntax_gui_tests -j4
xvfb-run -a ctest --test-dir build -R '^wx_syntax_gui$' --output-on-failure
```

The optional test exercises exact style IDs, font size and weight, foreground/background, strike, comments/strings isolation, unchanged document contents, and theme reset. It is not enabled by default because it requires a GUI display.
