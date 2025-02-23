#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <dlt/dlt_common.h>
#include "DLTViewer.h"
#include "utils.h"

std::vector<DLTInformation> parseDLTFile(const std::string source)
{
    int vflag = 0;
    DltFile file;
    dlt_file_init(&file, vflag);
    std::vector<DLTInformation> ret;
    int start = 0;
    int begin = 0;
    int end = 0;
    /* load, analyze data file and create index list */
    if (dlt_file_open(&file, source.c_str(), vflag) >= DLT_RETURN_OK)
    {
        end = file.counter_total;
        // printf("Total number of messages: %d %d %d\n", file.counter_total, file.counter, file.index);
        /// printf("Building index...\n");
        while (dlt_file_read(&file, vflag) >= DLT_RETURN_OK)
        {
            end++;
        }
        for (int num = begin; num < end; num++)
        {
            if (dlt_file_message(&file, num, vflag) < DLT_RETURN_OK)
                continue;

            if (0)
            {
                printf("\n%d ", num);
                char text[DLT_CONVERT_TEXTBUFSIZE + 1] = {0x00};
                if (dlt_message_print_hex(&(file.msg), text, DLT_CONVERT_TEXTBUFSIZE, vflag) < DLT_RETURN_OK)
                    continue;
            }
            {
                printf("\n%d ", num);
                char texth[DLT_CONVERT_TEXTBUFSIZE + 1] = {0x00};
                char text[DLT_CONVERT_TEXTBUFSIZE + 1] = {0x00};
                if (dlt_message_header(&(file.msg), texth, DLT_CONVERT_TEXTBUFSIZE, vflag) < DLT_RETURN_OK)
                    continue;

                printf("%s ", text);

                if (dlt_message_payload(&file.msg, text, DLT_CONVERT_TEXTBUFSIZE, DLT_OUTPUT_ASCII, vflag) < DLT_RETURN_OK)
                    continue;

                printf("[%s]\n", text);
                DLTInformation entry;
                entry.headerText = texth;
                entry.payload = text;
                entry.index = num;
                ret.emplace_back(entry);
            }
        }
        dlt_file_free(&file, vflag);
    }
    return ret;
}

std::vector<uint8_t> DLTViewerTab::getData()
{
    std::vector<uint8_t> ret;
    return ret;
}

Response DLTViewerTab::setData(std::vector<uint8_t> d)
{
    return Response::Success;
}

bool DLTViewerTab::wasChanged()
{
    return false;
}

Response DLTViewerTab::saveDocument()
{
    return Response::Success;
}

Response DLTViewerTab::openFile(std::string filepath)
{
    auto dltContent = parseDLTFile(filepath);
    for(auto it : dltContent){
      long index = dltentries->InsertItem(dltentries->GetItemCount(), std::to_string(it.index).c_str());
      dltentries->SetItem(index, 1, it.headerText);
      dltentries->SetItem(index, 2, it.payload);
    }
    if(dltContent.size()>0)
        return Response::Success;
    return Response::Error;
}

DLTViewerTab::DLTViewerTab(wxWindow *parent) : EditorTab(parent)
{
    sizer = new wxBoxSizer(wxVERTICAL);
    label = new wxStaticText(this, wxID_ANY, wxT("DLT Viewer"));

    dltentries = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);
    dltentries->InsertColumn(0, "Index", wxLIST_FORMAT_LEFT, 150);
    dltentries->InsertColumn(1, "Header", wxLIST_FORMAT_LEFT, 400);
    dltentries->InsertColumn(2, "payload", wxLIST_FORMAT_LEFT, 500);

    optionPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    openDLTFile = new wxButton(optionPanel, OpenDLTFile, wxT("Open a DLT File(read only, not filtering)"));
    wxBoxSizer *efimerus = new wxBoxSizer(wxHORIZONTAL);
    efimerus->Add(openDLTFile, 1, wxEXPAND | wxALL);
    optionPanel->SetSizer(efimerus);

    Bind(wxEVT_BUTTON, [this](wxCommandEvent &event)
         { 
        std::string path;
         if(OpenFileDialog(this, path, automotive_filter)==Response::Success && path.size()>0){
              this->openFile(path);
        } }, OpenDLTFile);

    sizer->Add(label, 0, wxEXPAND);
    sizer->Add(optionPanel, 0, wxEXPAND);
    sizer->Add(dltentries, 1, wxEXPAND);

    SetSizer(sizer);
}
