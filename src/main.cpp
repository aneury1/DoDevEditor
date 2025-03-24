 
#include <wx/wx.h>
#include "constant.h"
#include "frame.h"
#include "tree_sitter_api.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

// Declare Tree-sitter C++ parser
extern "C" {
    TSLanguage* tree_sitter_cpp();
}

// Read file content
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Extract function/class/struct declarations
void extractDeclarations(TSNode node, const std::string& source, std::vector<std::string>& declarations) {
    if (ts_node_is_null(node)) return;

    std::string nodeType = ts_node_type(node);
    
    // Extract function declaration
    if (nodeType == "function_declarator") {
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        declarations.push_back(source.substr(start, end - start));
    }

    // Extract class and struct declarations
    if (nodeType == "class_specifier" || nodeType == "struct_specifier") {
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        declarations.push_back(source.substr(start, end - start));
    }

    // Recursively traverse children
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        extractDeclarations(ts_node_child(node, i), source, declarations);
    }
}

// Main function to parse the C++ file
std::pair<std::string, std::vector<std::string>> parseFunctionClassStructureDeclaration(std::string file) {
    std::string code = readFile(file);
    if (code.empty()) return {file, {}};

    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_cpp());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, code.c_str(), code.size());
    TSNode root_node = ts_tree_root_node(tree);

    std::vector<std::string> declarations;
    extractDeclarations(root_node, code, declarations);

    // Cleanup
    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return {file, declarations};
}







void apiUsageTest()
{
        std::string filename = "/home/neon/Documents/projects/myprojects/DoDevEditor/src/TabContainer.cpp"; // Change this to your C++ file
    auto result = parseFunctionClassStructureDeclaration(filename);

    std::cout << "File: " << result.first << "\nDeclarations:\n";
    for (const auto& decl : result.second) {
        std::cout << decl << "\n";
    }
}




struct App : public wxApp{
  
    public:

    bool OnInit(){
        apiUsageTest();
        auto f = WindowFrame::get();
        f->Show(true);
        return true;
    }

};
 
wxIMPLEMENT_APP(App);
