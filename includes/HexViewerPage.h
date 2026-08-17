#pragma once

#if DODEV_ENABLE_HEX_VIEWER

#include <wx/panel.h>
#include <wx/string.h>

#include <cstddef>
#include <cstdint>
#include <vector>

class wxButton;
class wxCheckBox;
class wxChoice;
class wxStaticText;
class wxStyledTextCtrl;
class wxTextCtrl;

class HexViewerPage : public wxPanel
{
public:
    explicit HexViewerPage(wxWindow* parent, const wxString& filePath = wxString());

    bool OpenFile(const wxString& path, wxString* error = nullptr);
    bool Reload(wxString* error = nullptr);

    wxString GetTabTitle() const;
    const wxString& GetFilePath() const { return m_filePath; }

private:
    static constexpr std::uint64_t PAGE_SIZE = 64u * 1024u;

    wxString m_filePath;
    std::uint64_t m_fileSize = 0;
    std::uint64_t m_pageOffset = 0;
    std::uint64_t m_lastSearchOffset = 0;
    std::vector<unsigned char> m_pageBytes;

    wxTextCtrl* m_pathCtrl = nullptr;
    wxChoice* m_bytesPerRowChoice = nullptr;
    wxTextCtrl* m_offsetCtrl = nullptr;
    wxTextCtrl* m_searchCtrl = nullptr;
    wxCheckBox* m_hexSearch = nullptr;
    wxStyledTextCtrl* m_view = nullptr;
    wxButton* m_prevPage = nullptr;
    wxButton* m_nextPage = nullptr;
    wxStaticText* m_status = nullptr;

    void BuildUI();
    void ConfigureView();
    void BrowseFile();
    void GoToOffset();
    void FindNext();
    void ChangeBytesPerRow();

    bool LoadPage(std::uint64_t offset, wxString* error = nullptr);
    void RenderPage(std::uint64_t highlightOffset = UINT64_MAX, std::size_t highlightLength = 0);
    void UpdateStatus();
    void UpdateNavigation();

    int BytesPerRow() const;
    bool ParseOffset(const wxString& text, std::uint64_t& value) const;
    bool BuildSearchPattern(std::vector<unsigned char>& pattern, wxString* error) const;
    bool FindPattern(const std::vector<unsigned char>& pattern,
                     std::uint64_t startOffset,
                     std::uint64_t& foundOffset,
                     wxString* error) const;

    static wxString FormatOffset(std::uint64_t value, int width = 8);
    static wxString HumanSize(std::uint64_t bytes);
};

#endif // DODEV_ENABLE_HEX_VIEWER
