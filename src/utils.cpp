#include "utils.h"
#include "constant.h"
#include <fstream>
#include <sys/stat.h>

std::vector<uint8_t> read_binary_file(const std::string filename)
{
    std::vector<uint8_t> ret;

    // Open the file in binary mode
    std::fstream stream(filename, std::ios::in | std::ios::binary);
    if (!stream)
    {
        return ret;
        // throw std::runtime_error("Failed to open file: " + filename);
    }

    // Get the file size
    stream.seekg(0, std::ios::end);
    std::streamsize len = stream.tellg();
    if (len < 0)
    {
        throw std::runtime_error("Failed to get file size: " + filename +" "+ __FUNCTION__);
    }
    stream.seekg(0, std::ios::beg);

    ret.resize(len);

    if (!stream.read(reinterpret_cast<char *>(ret.data()), len))
    {
        return ret;
        /// throw std::runtime_error("Failed to read file: " + filename);
    }

    return ret;
}

std::string bytes_to_string(const std::vector<uint8_t> to)
{
    std::string s;
    for (auto it : to)
        s += it;
    return s;
}

wxString obtain_existing_file(std::string label, wxWindow *_this)
{

    wxFileDialog openFileDialog(_this, _(label), "", "",
                                file_filter,
                                wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return "";
    wxString file = openFileDialog.GetPath();
    return file;
}

wxString read_file(const std::string file)
{
    auto filev = read_binary_file(file);
    if (filev.size() > 0)
    {
        auto content = bytes_to_string(filev);
        wxString ret(content);
       /// std::cout <<"RED"<< ret.ToStdString()<<"\n";
        return ret;
    }
    return "";
}

bool question_yes_or_not(const wxString &label)
{
    int response = wxMessageBox(label, "Confirmation", wxYES_NO | wxICON_QUESTION, nullptr);

    if (response == wxYES)
    {
        return true;
    }

    return false;
}

bool write_content_to_file(const wxString& file, std::vector<uint8_t> content){
    std::fstream stream(file.ToAscii(), std::ios::out|std::ios::binary);
    if(stream.is_open()){
        const char *buffer = (const char *)content.data();;
        stream.write(buffer, content.size());
        stream.flush();
        stream.close();
        return true;
    }
    return false;
}
bool write_content_to_file(const wxString& file, const wxString& content){
    // Convert wxString to std::string
    std::string filePath = file.ToStdString();
    std::string fileContent = content.ToStdString();

    // Open the file in write mode
    std::fstream stream(filePath, std::ios::out | std::ios::binary);
    if (!stream.is_open()) {
        wxLogError("Failed to open file: %s", file);
        return false;
    }

    // Write the content to the file
    stream.write(fileContent.c_str(), fileContent.size());
    if (stream.fail()) {
        wxLogError("Error writing to file: %s", file);
        return false;
    }

    // Close the file
    stream.close();
    return true;
}


wxString create_new_file_with_content_with_dialog(wxWindow* parent, const wxString& defaultContent) {
    // Show the Save File dialog
    wxFileDialog saveFileDialog(
        parent,
        "Create a New File",
        wxEmptyString, // Default directory
        "new_file.txt", // Default file name
        file_filter,
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );
    std::cout <<" here ???";
    if (saveFileDialog.ShowModal() == wxID_CANCEL) {
        wxLogMessage("User canceled file creation.");
        return ""; // User canceled
    }

    // Get the file path
    wxString filePath = saveFileDialog.GetPath();

    // Create and write to the file
    std::ofstream file(filePath.ToStdString(), std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        wxLogError("Failed to create file: %s", filePath);
                return ""; // User canceled

    }

    // Write default content
    file.write(defaultContent.ToStdString().c_str(), defaultContent.size());
    if (file.fail()) {
        wxLogError("Failed to write to file: %s", filePath);
                return ""; // User canceled

    }

    file.close();
    wxLogMessage("File created successfully: %s", filePath);
    return filePath;
}


bool is_file(wxString path){
    struct stat path_stat;
    if (stat(path.c_str(), &path_stat) != 0) {
        return false; // Error accessing the path
    }
    return S_ISREG(path_stat.st_mode); // Check if it's a regular file
}