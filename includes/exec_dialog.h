#ifndef EXEC_DIALOG_H_DEFINED
#define EXEC_DIALOG_H_DEFINED
#include <wx/wx.h>
#include <wx/process.h>
#include <wx/textctrl.h>
#include <wx/filedlg.h>
#include <thread>
#include <atomic>
#include <mutex>

class exec_dialog :  public wxDialog {
public:
    exec_dialog(wxWindow* parent);
    ~exec_dialog();

private:
    wxTextCtrl* inputFileCtrl;
    wxTextCtrl* commandCtrl;
    wxTextCtrl* outputCtrl;
    std::thread executionThread;
    std::atomic<bool> stopFlag;
    std::mutex outputMutex;

    void on_browser(wxCommandEvent& event);

    void on_execute(wxCommandEvent& event);

    void execute_command(const std::string &command);
};



#endif 