#pragma once

#include "DiffEngine.h"

#include <wx/frame.h>
#include <wx/filepicker.h>

#include <cstddef>
#include <vector>

class wxCheckBox;
class wxFilePickerCtrl;
class wxSplitterWindow;
class wxStaticText;
class wxStyledTextCtrl;
class wxStyledTextEvent;

class MainFrame final : public wxFrame
{
public:
    enum class Side
    {
        Left,
        Right
    };

    MainFrame(const wxString& leftPath = {}, const wxString& rightPath = {});

    void SetFileForSide(bool leftSide, const wxString& path);

private:
    void BuildMenu();
    void BuildInterface();
    void ConfigureEditor(wxStyledTextCtrl* editor, bool leftSide);
    void BindEvents();

    void CompareFiles(bool showErrors = true);
    void RenderResult(const jld::DiffResult& result);
    void RenderSide(wxStyledTextCtrl* editor, const jld::DiffResult& result, Side side);
    void UpdateSummary(const jld::DiffResult& result);
    void ClearEditors();

    void OpenFile(Side side);
    void NavigateDifference(int direction);
    void ScrollBothToRow(std::size_t row);
    void SynchronizeScroll(wxStyledTextCtrl* source, wxStyledTextCtrl* target);

    [[nodiscard]] bool CanCompare() const;

    void OnOpenLeft(wxCommandEvent& event);
    void OnOpenRight(wxCommandEvent& event);
    void OnCompare(wxCommandEvent& event);
    void OnSwap(wxCommandEvent& event);
    void OnOptionChanged(wxCommandEvent& event);
    void OnFileChanged(wxFileDirPickerEvent& event);
    void OnPreviousDifference(wxCommandEvent& event);
    void OnNextDifference(wxCommandEvent& event);
    void OnLeftUpdateUi(wxStyledTextEvent& event);
    void OnRightUpdateUi(wxStyledTextEvent& event);
    void OnAbout(wxCommandEvent& event);

    wxFilePickerCtrl* m_leftPicker {nullptr};
    wxFilePickerCtrl* m_rightPicker {nullptr};
    wxCheckBox* m_ignoreTimestamp {nullptr};
    wxCheckBox* m_ignoreWhitespace {nullptr};
    wxCheckBox* m_ignoreCase {nullptr};
    wxCheckBox* m_autoCompare {nullptr};
    wxStyledTextCtrl* m_leftEditor {nullptr};
    wxStyledTextCtrl* m_rightEditor {nullptr};
    wxStaticText* m_summaryText {nullptr};
    wxSplitterWindow* m_splitter {nullptr};

    std::vector<std::size_t> m_differenceRows;
    std::size_t m_currentDifference {0};
    bool m_hasCurrentDifference {false};
    bool m_synchronizingScroll {false};
};
