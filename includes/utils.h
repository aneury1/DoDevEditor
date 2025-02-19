#ifndef __UTILS_H_DEFINED
#define __UTILS_H_DEFINED
#include <vector>
#include <stdint.h>
#include <string>
#include <fstream>
#include <wx/wx.h>
#include "constant.h"
///#include <system.h>
#include <stdlib.h>

enum class TTYTerminal{
    Konsole,
    Gnome,
    OtherLinux,
    MacOS,
    WindowCMD
};

static bool IsInstalled(const char* terminal) {
    std::string cmd = "which ";
    cmd += terminal;
    cmd += " > /dev/null 2>&1"; // Suppress output
    return system(cmd.c_str()) == 0;
}
static TTYTerminal GetTerminal(){
    system("x-terminal-emulator &");
    if (IsInstalled("konsole")) {
        std::cout << "Konsole is installed.\n";
        return TTYTerminal::Konsole;
        
    } else if (IsInstalled("gnome-terminal")) {
        std::cout << "GNOME Terminal is installed.\n";
        return TTYTerminal::Gnome;
    } else {
        std::cout << "Neither Konsole nor GNOME Terminal is installed.\n";
    }

    return TTYTerminal::OtherLinux;
}

static inline std::string ExtraOnlyFileName(const std::string& fullpath)
{
    int pos = fullpath.find_last_of(FileSeparator);
    if(pos==std::string::npos)
      return fullpath;
    
    return fullpath.substr(pos+1, fullpath.size()-pos);
}

static inline std::string ExtractFileExtension(const std::string& file)
{
    int pos = file.find_last_of(".");
    if(pos!=std::string::npos){ 
      return file;
    }
    return file.substr(pos+1, file.size()-pos);
}



static inline std::string toString(std::vector<uint8_t> content)
{
    std::string res;
    for (auto it : content)
    {
        res += static_cast<char>(it);
    }
    return res;
}

static inline std::vector<uint8_t> fromStrTo8Vec(std::string s){
    std::vector<uint8_t> ret;
    for(auto it : s)
       ret.emplace_back(static_cast<uint8_t>(it));
    return ret;
}




static Response QuestionYesOrNo(std::string title, std::string body, wxWindow *frame)
{
    int result = wxMessageBox(body.c_str(), title.c_str(), wxYES_NO | wxICON_QUESTION, frame);
    if (result == wxYES)
    {
        return Response::Success;
    }
    else
    {
        return Response::Cancel;
    }
}

static inline std::string CreateFileBySelectingIt(wxWindow *parent, const std::string &defaultContent)
{
    // Show the Save File dialog
    wxFileDialog saveFileDialog(
        parent,
        "Create a New File",
        wxEmptyString,  // Default directory
        "new_file.txt", // Default file name
        file_filter,
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (saveFileDialog.ShowModal() == wxID_CANCEL)
    {
        wxLogMessage("User canceled file creation.");
        return ""; // User canceled
    }

    // Get the file path
    std::string filePath = saveFileDialog.GetPath().ToStdString();

    // Create and write to the file
    std::ofstream file(filePath.c_str(), std::ios::out | std::ios::binary);
    if (!file.is_open())
    {
        wxLogError("Failed to create file: %s", filePath);
        return ""; // User canceled
    }

    // Write default content
    file.write(defaultContent.c_str(), defaultContent.size());
    if (file.fail())
    {
        wxLogError("Failed to write to file: %s", filePath);
        return ""; // User canceled
    }

    file.close();
    wxLogMessage("File created successfully: %s", filePath);
    return filePath;
}

static Response WriteContentToFile(std::string filePath, std::string content)
{
    // Create and write to the file
    std::ofstream file(filePath.c_str(), std::ios::out | std::ios::binary);
    if (!file.is_open())
    {
        wxLogError("Failed to create file: %s", filePath);
        return Response::Cancel; // User canceled
    }

    // Write default content
    file.write(content.c_str(), content.size());
    if (file.fail())
    {
        ////  wxLogError("Failed to write to file: %s", filePath);
        return Response::Cancel; // User canceled
    }

    file.close();
    wxLogMessage("File created successfully: %s", filePath);
    return Response::Success;
}

static std::vector<uint8_t> ReadBinaryFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate); // Open in binary mode, move to the end

    if (!file)
    {

        return {};
    }

    std::streamsize size = file.tellg(); // Get file size
    file.seekg(0, std::ios::beg);        // Rewind to the beginning

    std::vector<uint8_t> buffer(size);      // Allocate buffer
    file.read((char *)buffer.data(), size); // Read file into buffer

    return buffer; // Return file contents
}

static std::string ReadFile(std::string file)
{
    auto fsbuffer = ReadBinaryFile(file);
    auto s = toString(fsbuffer);
    return s;
}

static Response OpenFileDialog(wxWindow *parent, std::string &file)
{
    wxFileDialog openFileDialog(parent, "Open a file", "", "", file_filter,
                                wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
    {
        return Response::Cancel; // User canceled
    }

    wxString filePath = openFileDialog.GetPath();

    std::fstream fsfile(filePath.ToStdString().c_str(), std::ios::in);
    if (fsfile.is_open())
    {
        file = filePath.ToStdString();
        return Response::Success;
        /// wxString content;
        /// file.ReadAll(&content);
        ///  txtContent->SetValue(content);
    }
    return Response::Error;
}

#endif ///__UTILS_H_DEFINED