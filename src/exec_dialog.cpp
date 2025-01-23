#include "exec_dialog.h"
#include <wx/sstream.h>

exec_dialog::exec_dialog(wxWindow *parent)
      : wxPanel(parent, wxID_ANY)
   // : wxPanel(parent, wxID_ANY, "Execute Command", wxDefaultPosition, wxSize(600, 400))
{
    // Create controls
    wxStaticText *inputLabel = new wxStaticText(this, wxID_ANY, "Select Current Dir:");
    inputFileCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(400, -1));
    wxButton *browseButton = new wxButton(this, wxID_ANY, "Browse");
    wxStaticText *commandLabel = new wxStaticText(this, wxID_ANY, "Command:");
    commandCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(400, -1));
    wxButton *executeButton = new wxButton(this, wxID_ANY, "Execute");
    outputCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 200),
                                wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH);

    // Layout
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *inputSizer = new wxBoxSizer(wxHORIZONTAL);
    inputSizer->Add(inputLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    inputSizer->Add(inputFileCtrl, 1, wxALL | wxEXPAND, 5);
    inputSizer->Add(browseButton, 0, wxALL, 5);
    wxBoxSizer *commandSizer = new wxBoxSizer(wxHORIZONTAL);
    commandSizer->Add(commandLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    commandSizer->Add(commandCtrl, 1, wxALL | wxEXPAND, 5);
    commandSizer->Add(executeButton, 0, wxALL, 5);
    mainSizer->Add(inputSizer, 0, wxEXPAND);
    mainSizer->Add(commandSizer, 0, wxEXPAND);
    mainSizer->Add(outputCtrl, 1, wxALL | wxEXPAND, 5);
    SetSizer(mainSizer);

    // Event bindings
    browseButton->Bind(wxEVT_BUTTON, &exec_dialog::on_browser, this);
    executeButton->Bind(wxEVT_BUTTON, &exec_dialog::on_execute, this);

    stopFlag = false;
}

exec_dialog::~exec_dialog()
{
    stopFlag = true;
    if (executionThread.joinable())
        executionThread.join();
}

void exec_dialog::on_browser(wxCommandEvent &event)
{
    wxDirDialog openFolderDialog(this,
                                "Select a Folder to Open", "",
                                wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

    if (openFolderDialog.ShowModal() == wxID_OK)
    {
        inputFileCtrl->SetValue(openFolderDialog.GetPath());
        outputCtrl->AppendText("path of execution would be: "+openFolderDialog.GetPath()+" \n");
    }
}

void exec_dialog::on_execute(wxCommandEvent &event)
{
    wxString inputFilePath = inputFileCtrl->GetValue();
    wxString command = commandCtrl->GetValue();

    if (/*inputFilePath.IsEmpty() ||*/ command.IsEmpty())
     {
        wxMessageBox("Please specify both an input file and a command.", "Error", wxICON_ERROR);
        return;
     }
    
    
    if(!inputFilePath.IsEmpty())
    {
        command = "cd " + inputFilePath +" && "+ command; 
        ///command += " " + inputFilePath;
    }
    outputCtrl->Clear();
    stopFlag = false;

    if (executionThread.joinable())
        executionThread.join();

    executionThread = std::thread(&exec_dialog::execute_command, this, std::string(command.mb_str()));
}

void exec_dialog::execute_command(const std::string &command)
{
    std::unique_lock<std::mutex> lock(outputMutex, std::defer_lock);
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
    {
        wxMessageBox("Failed to execute command.", "Error", wxICON_ERROR);
        return;
    }

    char buffer[128];
    while (!stopFlag && fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        lock.lock();
        wxString output(buffer, wxConvUTF8);
        CallAfter([this, output]()
                    { outputCtrl->AppendText(output); });
        lock.unlock();
    }

    pclose(pipe);

    if (!stopFlag)
    {
      //  CallAfter([this]()
      //              { wxMessageBox("Command execution completed.", "Info", wxICON_INFORMATION); });
    }
}