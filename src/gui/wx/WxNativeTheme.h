#pragma once
// This header must compile independently of the including file.
#include <wx/wx.h>
// Optional GTK3 application-local CSS. No OS theme files are modified.
#if defined(DODEV_GTK3_THEME) && defined(__WXGTK__)
#include <gtk/gtk.h>
#endif
namespace wxui {
inline wxString cssColor(const wxColour& c) {
  return wxString::Format("#%02x%02x%02x", int(c.Red()), int(c.Green()),
                          int(c.Blue()));
}
inline bool isDark(const wxColour& c) {
  return (299 * int(c.Red()) + 587 * int(c.Green()) + 114 * int(c.Blue())) <
         128000;
}
inline wxColour contrastText(const wxColour& c) {
  return isDark(c) ? *wxWHITE : *wxBLACK;
}
inline void applyNativeTheme(const wxColour& window, const wxColour& panel,
                             const wxColour& input, const wxColour& control,
                             const wxColour& text, const wxColour& accent,
                             const wxColour& border) {
#if defined(DODEV_GTK3_THEME) && defined(__WXGTK__)
  auto* screen = gdk_screen_get_default();
  if (!screen) return;
  auto* settings = gtk_settings_get_default();
  if (settings)
    g_object_set(settings, "gtk-application-prefer-dark-theme",
                 gboolean(isDark(window)), nullptr);
  // CSS is rebuilt for each palette, using validated RGB values only.
  const wxString w = cssColor(window), p = cssColor(panel), i = cssColor(input),
                 c = cssColor(control), t = cssColor(text),
                 a = cssColor(accent), b = cssColor(border),
                 sel = cssColor(contrastText(accent));
  wxString css;
  auto rule = [&](const wxString& selector, const wxString& bg,
                  const wxString& fg) {
    css += selector + " { background-color: " + bg + "; color: " + fg +
           "; background-image: none; border-color: " + b + "; }\n";
  };
  rule("window, dialog, messagedialog, filechooser, headerbar, titlebar", w, t);
  rule(
      "menubar, menu, menuitem, toolbar, statusbar, notebook, notebook > "
      "header, notebook > stack, frame, popover",
      p, t);
  rule(
      "notebook > header tab, button, combobox button, spinbutton button, "
      "checkbutton, radiobutton",
      c, t);
  rule(
      "entry, spinbutton, textview, textview text, treeview, list, list row, "
      "scrolledwindow viewport",
      i, t);
  rule("notebook > header tab:checked", i, t);
  rule(
      "menuitem:hover, button:hover, notebook > header tab:hover, "
      "treeview:selected, treeview:selected:focus, list row:selected, textview "
      "text selection, entry selection",
      a, sel);
  rule("scrollbar, scrollbar trough, separator, paned separator", p, t);
  rule("scrollbar slider", c, t);
  rule("tooltip, tooltip background", c, t);
  css += "label { color: " + t + "; }\n";
  css +=
      "menuitem:hover label, button:hover label, row:selected label { color: " +
      sel + "; }\n";
  css += "*:disabled { opacity: 0.65; }\n";
  static GtkCssProvider* provider = nullptr;
  if (!provider) {
    provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(
        screen, GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
  auto utf = css.ToUTF8();
  GError* error = nullptr;
  gtk_css_provider_load_from_data(provider, utf.data(),
                                  static_cast<gssize>(utf.length()), &error);
  if (error) {
    wxLogWarning("GTK theme CSS: %s", wxString::FromUTF8(error->message));
    g_error_free(error);
  }
#else
  (void)window;
  (void)panel;
  (void)input;
  (void)control;
  (void)text;
  (void)accent;
  (void)border;
#endif
}
}  // namespace wxui
