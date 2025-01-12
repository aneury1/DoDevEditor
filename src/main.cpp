#include "main_frame.h"

class DoDevEditorApp : public wxApp {
public:
    virtual bool OnInit() {
        auto frame = main_window_frame::get(); ///new main_window_frame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(DoDevEditorApp);


#if 0
#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <functional>

// Function to execute a shell command and stream its output
void executeAndStreamOutput(const std::string& command, std::function<void(const std::string&)> callback) {
    // Open a pipe to the command using popen()
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("Failed to open pipe");
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        // Stream the output to the callback function
        callback(buffer);
    }
}

// Example callback function
void outputCallback(const std::string& line) {
    // Process or display the streamed output
    std::cout << "Output: " << line;
}

int main() {
    try {
        std::string command = "sudo apt-get update"; // Example command
        executeAndStreamOutput(command, outputCallback);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}


#endif
#if 0
#include <wx/wx.h>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

// Application class
class MyApp : public wxApp {
public:
    virtual bool OnInit();
};

// Frame class
class MyFrame : public wxFrame {
public:
    MyFrame();

private:
    wxTextCtrl* outputCtrl;

    void executeAndStreamOutput(const std::string& command);
    void appendOutput(const std::string& line);
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit() {
    MyFrame* frame = new MyFrame();
    frame->Show(true);
    return true;
}

MyFrame::MyFrame()
    : wxFrame(nullptr, wxID_ANY, "Shell Command Output", wxDefaultPosition, wxSize(600, 400)) {
    // Create a multi-line text control to display output
    outputCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

    // Start streaming output in a separate thread
    std::thread([this]() {
        executeAndStreamOutput("cd test && ./dodeveditor_test"); // Example command
    }).detach();
}

void MyFrame::executeAndStreamOutput(const std::string& command) {
    // Open a pipe to the command
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        appendOutput("Failed to execute command.\n");
        return;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        std::string line(buffer);
        appendOutput(line); // Append the output line to the text control
    }
    appendOutput("Test Finish");
}

void MyFrame::appendOutput(const std::string& line) {
    // Safely update the wxTextCtrl from the main GUI thread
    CallAfter([this, line]() {
        outputCtrl->AppendText(line);
    });
}


#endif