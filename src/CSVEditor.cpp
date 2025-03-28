#include "CSVEditor.h"
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>

#include "utils.h"

namespace
{

    inline static std::string ReadFile(std::string file)
    {
        std::string ret;
        std::fstream stream(file, std::ios::in | std::ios::binary | std::ios::ate);
        int len = stream.tellg();
        stream.seekg(0, std::ios::beg);
        ret.reserve(len);
        std::cout << len << "\n";
        char *buffer = ret.data();
        stream.read(buffer, len);
        stream.close();
        std::cout << stream.gcount() << buffer << "\n";

        return std::string(buffer);
    }

    inline static std::vector<std::string> splitCSV(const std::string &line, char delimiter = ',')
    {
        std::vector<std::string> result;
        std::stringstream ss(line);
        std::string token;

        while (std::getline(ss, token, delimiter))
        {
            result.push_back(token);
        }

        return result;
    }

    inline static std::vector<std::vector<std::string>> parseCSV(std::string file)
    {
        auto content = ReadFile(file);
        std::cout << content << "\n";
        std::stringstream ss(content);
        std::string line;
        std::vector<std::vector<std::string>> ret;
        char delim = '\n';
        while (std::getline(ss, line, delim))
        {
            auto entryLine = splitCSV(line);
            ret.push_back(entryLine);
            line = "";
        }
        return ret;
    }

}

CSVEditor::CSVEditor(wxWindow *parent) : EditorTab(parent)
{
    sizer= new wxBoxSizer(wxVERTICAL);
    openFileButton = new wxButton(this, OpenCSVFile,wxT("Open CSV File"));
    sizer->Add(openFileButton, 0);
   
       Bind(wxEVT_BUTTON, [this](wxCommandEvent &event)
         { 
        std::string path;
         if(OpenFileDialog(this, path, csv_filter)==Response::Success && path.size()>0){
              this->openFile(path);
        } }, OpenCSVFile);
   
        SetBackgroundColour(defaultSettings.getPanelBG());
  
    SetSizer(sizer);   
}


int CSVEditor::getTypeOfEditor()
{
    return 0;
}

std::vector<uint8_t> CSVEditor::getData() {
    return {};
}

Response CSVEditor::setData(std::vector<uint8_t> d) {
    return Response::Success;
}

bool CSVEditor::wasChanged() {
    return false;
}

Response CSVEditor::saveDocument() {
    return Response::Success;

}

Response CSVEditor::openFile(std::string filepath)
{
    auto el = parseCSV(filepath.size()<=0?"./sample.csv":filepath.c_str());
    int rows = el.size();
    int cols = 0;
    if (rows > 0)
    {
        cols = el[0].size();
    }
    grid = new wxGrid(this,
                      -1,
                      wxDefaultPosition,
                      wxSize(800,600));
    grid->CreateGrid(rows, cols);
    grid->SetRowSize(0, 60);
    grid->SetColSize(0, 120);

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            auto ele = el[r];
            auto str = ele[c];
            grid->SetCellValue(r, c, str.c_str());
        }
    }
    sizer->Add(grid, 1, wxEXPAND|wxALL);
    Refresh();

    return Response::Success;
}