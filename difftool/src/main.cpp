#include "MainFrame.h"

#include <wx/app.h>
#include <wx/image.h>

class JournalLogDiffApp final : public wxApp
{
public:
    bool OnInit() override
    {
        if (!wxApp::OnInit())
        {
            return false;
        }

        wxInitAllImageHandlers();

        wxString leftPath;
        wxString rightPath;
        if (argc >= 2)
        {
            leftPath = argv[1];
        }
        if (argc >= 3)
        {
            rightPath = argv[2];
        }

        auto* frame = new MainFrame(leftPath, rightPath);
        frame->Show(true);
        SetTopWindow(frame);
        return true;
    }
};

wxIMPLEMENT_APP(JournalLogDiffApp);
