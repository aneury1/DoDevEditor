#if 0 
#include <wx/wx.h>
#include "constant.h"
#include "frame.h"

struct App : public wxApp{
  
    public:

    bool OnInit(){
 
        auto f = WindowFrame::get();
        f->Show(true);
        return true;
    }

};
 
wxIMPLEMENT_APP(App);
#endif

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <tree_sitter/api.h>
#include <fstream>
#include <sstream>
#include <iostream>

// Declare Tree-sitter parser function
extern "C" {
    TSLanguage* tree_sitter_cpp();
}

// Function to read file content
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) return "Error: Cannot open file";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Function to parse C++ code
void parseCppCode(const std::string& code, wxListCtrl* listCtrl) {
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_cpp());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, code.c_str(), code.size());
    TSNode root_node = ts_tree_root_node(tree);

    // Display root node type
    wxListItem item;
    item.SetId(0);
    item.SetText(wxString::FromUTF8(ts_node_type(root_node)));
    listCtrl->InsertItem(item);

    // Cleanup
    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// wxWidgets Frame
class MyFrame : public wxFrame {
public:
    MyFrame() : wxFrame(nullptr, wxID_ANY, "Tree-Sitter C++ Parser", wxDefaultPosition, wxSize(500, 400)) {
        wxPanel* panel = new wxPanel(this);
        listCtrl = new wxListCtrl(panel, wxID_ANY, wxPoint(10, 10), wxSize(480, 300), wxLC_REPORT);
        listCtrl->InsertColumn(0, "Parsed Structure", wxLIST_FORMAT_LEFT, 400);

        wxButton* parseButton = new wxButton(panel, wxID_ANY, "Parse C++", wxPoint(10, 320), wxSize(100, 30));
        parseButton->Bind(wxEVT_BUTTON, &MyFrame::OnParseClicked, this);
    }

private:
    wxListCtrl* listCtrl;

    void OnParseClicked(wxCommandEvent&) {
        std::string code = readFile("example.cpp"); // Change to your C++ file
        parseCppCode(code, listCtrl);
    }
};

// wxWidgets App
class MyApp : public wxApp {
public:
    bool OnInit() override {
        MyFrame* frame = new MyFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);
