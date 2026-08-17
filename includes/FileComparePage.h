#pragma once

#if DODEV_ENABLE_FILE_COMPARE

#include <wx/panel.h>
#include <wx/string.h>

#include <cstddef>
#include <cstdint>
#include <vector>

class wxButton;
class wxCheckBox;
class wxChoice;
class wxSplitterWindow;
class wxStaticText;
class wxStyledTextCtrl;
class wxTextCtrl;

class FileComparePage : public wxPanel
{
public:
    explicit FileComparePage(wxWindow* parent,
                             const wxString& leftPath = wxString(),
                             const wxString& rightPath = wxString());

    wxString GetTabTitle() const;
    const wxString& GetLeftPath() const { return m_leftPath; }
    const wxString& GetRightPath() const { return m_rightPath; }

private:
    enum class CompareMode
    {
        Auto,
        Text,
        Binary
    };

    enum class RowKind
    {
        Equal,
        Changed,
        LeftOnly,
        RightOnly
    };

    struct TextRow
    {
        wxString left;
        wxString right;
        int leftLine = 0;
        int rightLine = 0;
        RowKind kind = RowKind::Equal;
    };

    wxString m_leftPath;
    wxString m_rightPath;

    wxTextCtrl* m_leftPathCtrl = nullptr;
    wxTextCtrl* m_rightPathCtrl = nullptr;
    wxChoice* m_modeChoice = nullptr;
    wxCheckBox* m_ignoreWhitespace = nullptr;
    wxCheckBox* m_ignoreCase = nullptr;
    wxButton* m_prevButton = nullptr;
    wxButton* m_nextButton = nullptr;
    wxStyledTextCtrl* m_leftEditor = nullptr;
    wxStyledTextCtrl* m_rightEditor = nullptr;
    wxStaticText* m_status = nullptr;

    std::vector<int> m_differenceRows;
    int m_currentDifference = -1;
    bool m_syncingScroll = false;

    void BuildUI();
    void ConfigureEditor(wxStyledTextCtrl* editor, bool rightSide);
    void BrowseLeft();
    void BrowseRight();
    void SwapFiles();
    void CompareFiles();
    CompareMode SelectedMode() const;

    bool ReadFileBytes(const wxString& path,
                       std::vector<unsigned char>& bytes,
                       wxString* error) const;
    bool LooksBinary(const std::vector<unsigned char>& bytes) const;
    wxString DecodeText(const std::vector<unsigned char>& bytes) const;

    void CompareText(const std::vector<unsigned char>& leftBytes,
                     const std::vector<unsigned char>& rightBytes);
    void CompareBinary(const std::vector<unsigned char>& leftBytes,
                       const std::vector<unsigned char>& rightBytes);

    std::vector<TextRow> BuildAlignedTextRows(const wxString& leftText,
                                              const wxString& rightText) const;
    std::vector<wxString> SplitLines(const wxString& text) const;
    wxString ComparisonKey(const wxString& value) const;

    void SetEditorsText(const wxString& leftText, const wxString& rightText);
    void MarkRow(int row, RowKind kind);
    void ClearMarkers();
    void NavigateDifference(int delta);
    void ScrollToDifference(int row);
    void SyncScroll(wxStyledTextCtrl* source, wxStyledTextCtrl* target);
    void UpdateNavigation();
    void UpdateCaptions();

    static wxString FormatTextLine(int sourceLine, const wxString& text);
    static wxString FormatHexLine(size_t offset,
                                  const std::vector<unsigned char>& bytes,
                                  size_t start,
                                  size_t count);
};

#endif // DODEV_ENABLE_FILE_COMPARE
