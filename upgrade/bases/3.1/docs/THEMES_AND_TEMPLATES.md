# Theme Designer and File Templates

## Theme formats

Embedded presets are stored as `std::string_view` C++ literals in `resources/EmbeddedUi.h`; the settings, theme designer and template windows are embedded in `resources/EmbeddedSettingsUi.h`. **No XML files are required for the built-in GUI.**

The 13 built-in presets include all 11 previous themes and two additional VS Code-like presets: `VS Code Dark Modern` and `VS Code Light Modern`. Palette inspirations are visual references; assets or code from VS Code are not bundled.

The new `ThemeDefinition` model controls theme name, Raven base `Dark`/`Light`/`GlassDark`/`GlassLight`, accent, colors for window/panel/control/input/text/separator, syntax keyword/string/comment, and widget corner radius. The Designer can preview this model and export the result as JSON or Raven theme XML to an arbitrary user-selected path.

### JSON theme

```json
{
  "formatVersion": 1,
  "name": "My Midnight",
  "base": "Dark",
  "accent": "#5599EE",
  "cornerRadius": 10,
  "colors": {
    "window": "#181818",
    "panel": "#252526",
    "control": "#333337",
    "input": "#1B1B1F",
    "text": "#E6E6E6",
    "separator": "#444444"
  },
  "syntax": {
    "keyword": "#569CD6",
    "string": "#CE9178",
    "comment": "#6A9955"
  }
}
```

The same model can be exported as a Raven `<Theme>` document. Advanced original Raven XML styles are preserved **when using the original XML form**; the current simplified JSON theme schema does not model every advanced property (such as arbitrary extra `<Style>` selectors, material parameters or animation properties). Users who need full round-trip control should retain XML.

Custom theme folders are optional. Theme import/export dialogs let users choose any filename; the default discovery folder is `<config>/themes`. Invalid colors are rejected before import. Saved custom themes do not replace the embedded theme source.

## File templates

The template engine (`include/dodev/templates/TemplateRegistry.h`) exposes built-in C++17 source/header, Python, JSON, XML and CMake templates. From **File → New from Template**, pick a template, set the filename, preview and create a new unsaved tab. The following placeholders can be replaced in template text:

`FILE_NAME`, `CLASS_NAME`, `PROJECT_NAME`, `DATE`, `AUTHOR`.

Create an optional templates directory containing `templates.json` and referenced `.tpl` files:

```json
{
  "templates": [
    {"id":"sql-query","name":"SQL Query","extension":".sql","file":"sql.tpl"}
  ]
}
```

Configure extension mappings in `settings.json` under `templates.associations`, for example `".sql": "sql-query"`. Templates supplied by plugins use the same registry. Missing files are skipped; default templates continue to work.
