#ifndef PARSEHEADERS_H_DEFINED
#define PARSEHEADERS_H_DEFINED

#include <iostream>
#include <fstream>
#include <regex>
#include <string>

static inline void ParseHeaderFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: Could not open file!" << std::endl;
        return;
    }

    std::string line;
    std::regex function_regex(R"((\w[\w\s*&]+)\s+(\w+)\s*\(([^)]*)\)\s*;)");
    std::smatch match;

    std::cout << "Found function declarations:\n";
    while (std::getline(file, line)) {
        if (std::regex_search(line, match, function_regex)) {
            std::cout << "Return Type: " << match[1] << "\n";
            std::cout << "Function Name: " << match[2] << "\n";
            std::cout << "Parameters: " << match[3] << "\n\n";
        }
    }

    file.close();
}


#endif