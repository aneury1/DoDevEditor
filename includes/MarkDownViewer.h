#ifndef MARKDOWNVIEWER_H
#define MARKDOWNVIEWER_H
 
#include <wx/wx.h>
class MarkdownView : public wxPanel
{
public:
    MarkdownView(wxWindow* parent){}

    void LoadMarkdown(const wxString& markdown){}

private:
    
};

#endif // MARKDOWNVIEWER_H