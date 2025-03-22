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
#include <iostream>
#include <regex>
#include <string>
#include <wx/sckaddr.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

//#include <type_traits>

//template<typename T, typename O>
//struct IsTypeOf {
//    static constexpr bool value = std::is_same_v<T, O>;
//};
/*
template<typename T, typename O>
struct IsTypeOf {
    static constexpr bool value = false;
};

// Specialization when T and O are the same type
template<typename T>
struct IsTypeOf<T, T> {
    static constexpr bool value = true;
};
*/
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

static Response OpenFileDialog(wxWindow *parent, std::string &file, const char *filter=nullptr)
{
    wxFileDialog openFileDialog(parent, "Open a file", "", "",(filter!=nullptr?filter:file_filter),
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


static inline bool validateExpression(std::string text, std::string regexPattern) {
    try {
        // Create a regex object from the provided pattern
        std::regex pattern(regexPattern);
        
        // Use regex_match to check if the entire text matches the pattern
        return std::regex_match(text, pattern);
    }
    catch (const std::regex_error& e) {
        // Handle invalid regex pattern
        std::cerr << "Regex error: " << e.what() << std::endl;
        return false;
    }
}

#include <iostream>
#include <vector>
#include <string>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> 
struct IPInfo {
    std::string interfaceName;
    std::string ipAddress;
    std::string hostName;
};

static inline std::vector<IPInfo> GetLocalIPs2() {
    std::vector<IPInfo> ipList;
    struct ifaddrs *interfaces = nullptr, *ifa = nullptr;

    if (getifaddrs(&interfaces) == -1) {
        perror("getifaddrs");
        return ipList;
    }

    char host[NI_MAXHOST];
    for (ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue; // Skip interfaces without address

        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {  // IPv4 or IPv6
            void* addr;
            char ipStr[INET6_ADDRSTRLEN];

            if (family == AF_INET) { // IPv4
                addr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            } else { // IPv6
                addr = &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr;
            }

            inet_ntop(family, addr, ipStr, sizeof(ipStr));

            // Get hostname
            if (getnameinfo(ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                            host, NI_MAXHOST, nullptr, 0, NI_NAMEREQD) != 0) {
                snprintf(host, sizeof(host), "Unknown");
            }

            ipList.push_back({ifa->ifa_name, ipStr, host});
        }
    }

    freeifaddrs(interfaces);
    return ipList;
}


static inline wxArrayString GetAllLocalIPs() {
    wxArrayString ips;
    struct addrinfo hints{}, *res, *p;
    hints.ai_family = AF_INET;  // IPv4 only
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, "0", &hints, &res) != 0) {
        return ips;
    }

    for (p = res; p != nullptr; p = p->ai_next) {
        struct sockaddr_in* addr = (struct sockaddr_in*)p->ai_addr;
        ips.Add(wxString::FromUTF8(inet_ntoa(addr->sin_addr)));
    }

    freeaddrinfo(res);
    return ips;
}

static inline std::string ExtractFileOnly(const std::string filePa){
    
    int pos = filePa.find_last_of(FileSeparator);
    if(pos!= std::string::npos){
        return filePa.substr(pos+1, filePa.size()-pos);
    }
  return filePa;
}

static inline std::string ExtractPathOnly(const std::string filePa){
    
    int pos = filePa.find_last_of(FileSeparator);
    if(pos!= std::string::npos){
        return filePa.substr(0, pos);
    }
  return filePa;
}

template<typename T, typename O>
struct IsTypeOf{
    bool value = false;
};
 


#endif ///__UTILS_H_DEFINED