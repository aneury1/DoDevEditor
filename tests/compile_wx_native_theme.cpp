// Deliberately include this header first: no wxWidgets precompiled header.
#include <type_traits>
#include <utility>

#include "../src/gui/wx/WxNativeTheme.h"

static_assert(
    std::is_same_v<decltype(wxui::cssColor(std::declval<const wxColour&>())),
                   wxString>);
static_assert(std::is_same_v<
              decltype(wxui::contrastText(std::declval<const wxColour&>())),
              wxColour>);
