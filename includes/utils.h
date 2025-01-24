#ifndef UTILS_H_DEFINED
#define UTILS_H_DEFINED
#include <vector>
#include <cstdint>
#include <string>
#include <iostream>
#include <wx/wx.h>

std::vector<uint8_t> read_binary_file(const std::string f);

std::string bytes_to_string(const std::vector<uint8_t> to);

wxString obtain_existing_file(std::string label, wxWindow *_this);

wxString read_file(const std::string file);

bool question_yes_or_not(const wxString& label);

bool write_content_to_file(const wxString& file, std::vector<uint8_t> content);

bool write_content_to_file(const wxString& file, const wxString& content);

wxString create_new_file_with_content_with_dialog(wxWindow* parent, const wxString& defaultContent);


#endif // UTILS_H_DEFINED
