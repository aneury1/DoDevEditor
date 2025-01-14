
#include "main_frame.h"

class DoDevEditorApp : public wxApp {
public:
    virtual bool OnInit() {
        auto frame = do_editor::get(); ///new do_editor();
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

 
#if 0
#include <iostream>
#include <string>
#include <sstream>
#include <cstdio> // For popen and pclose
/// echo '#include <iostream> int main() { std::cout<<"Hello, World!"<<std::endl;return 0; }' | clang-format-14

std::string ApplyClangFormat(const std::string& code) {
    // Command to invoke clang-format
    const std::string clangFormatCommand = " echo \'#include <iostream> int main() { std::cout<<\"Hello, World!\"<<std::endl;return 0; }\' | clang-format-14";

    // Open a process to write to clang-format's stdin and read its stdout
    FILE* pipe = popen(clangFormatCommand.c_str(), "w+");
    if (!pipe) {
        throw std::runtime_error("Failed to open pipe to "+ clangFormatCommand);
    }

    // Write the input C++ code to clang-format's stdin
    fwrite(code.c_str(), sizeof(char), code.size(), pipe);
    fflush(pipe); // Ensure all input is sent to clang-format

    // Read the formatted output from clang-format's stdout
    std::ostringstream formattedCode;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        formattedCode << buffer;
    }

    // Close the pipe
    int exitCode = pclose(pipe);
    if (exitCode != 0) {
        throw std::runtime_error("clang-format failed with exit code: " + std::to_string(exitCode));
    }

    return formattedCode.str();
}

int main() {
    // Example unformatted code
    std::string unformattedCode = R"(
#include <iostream>
int main() { std::cout << "Hello, World!" << std::endl; return 0; }
)";

    try {
        // Format the code using clang-format
        std::string formattedCode = ApplyClangFormat(unformattedCode);

        // Print the formatted code
        std::cout << "Formatted Code:\n" << formattedCode << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}

#endif