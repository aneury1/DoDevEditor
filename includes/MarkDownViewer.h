#ifndef MARKDOWNVIEWER_H
#define MARKDOWNVIEWER_H
#include <wx/webview.h>
#include <wx/wx.h>
class MarkdownView : public wxPanel
{
public:
    MarkdownView(wxWindow* parent);

    void LoadMarkdown(const wxString& markdown);

private:
    wxWebView* web;
};

#endif // MARKDOWNVIEWER_H