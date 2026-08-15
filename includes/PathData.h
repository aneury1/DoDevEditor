#ifndef PATHDATA_H
#define PATHDATA_H  
#include <wx/treectrl.h>
#include <wx/string.h>
// ─────────────────────────────────────────────────────────────────────────────
// PathData – wxTreeItemData carrying a filesystem path
// (wxStringClientData is NOT a wxTreeItemData subclass, so we roll our own)
// ─────────────────────────────────────────────────────────────────────────────

class PathData : public wxTreeItemData {
public:
    explicit PathData(const wxString& path) : m_path(path) {}
    const wxString& GetPath() const { return m_path; }
private:
    wxString m_path;
};

#endif // PATHDATA_H