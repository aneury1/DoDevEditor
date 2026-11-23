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
        std::cout <<"File Count=>"<< file.counter<<"\n";
        // printf("Total number of messages: %d %d %d\n", file.counter_total, file.counter, file.index);
        /// printf("Building index...\n");
        while (dlt_file_read(&file, vflag) >= DLT_RETURN_OK)
        {
            end++;
        }
        std::cout <<"File Counter=>"<< file.counter_total<<"   end"<<end<<"\n";

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
                printf("Here only\n");
                DltMessage msg;
                dlt_message_init(&msg, vflag);
                char text[DLT_CONVERT_TEXTBUFSIZE + 1] = {0x00};
                 dlt_message_header(&file.msg,  text, DLT_CONVERT_TEXTBUFSIZE, vflag);
                 dlt_message_print_header(&file.msg,  text, DLT_CONVERT_TEXTBUFSIZE, vflag);
                
            }
            if(1)
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

                entry.timestamp = std::to_string(file.msg.storageheader->microseconds)+
                " "+std::to_string(file.msg.storageheader->seconds);
                entry.apid = file.msg.extendedheader->apid;
                entry.context = file.msg.extendedheader->ctid;
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


wxColour getBackgroundByAPIDHardcode(std::string path)
{
////"KER","SYS","WIF","CON"
   if(path == "KERN")return *wxRED;
   if(path == "CON")return *wxGREEN;
   if(path == "SYS")return *wxCYAN;
   if(path == "WIF")return *wxYELLOW;
   return *wxWHITE;
}

#include <map>
Response DLTViewerTab::openFile(std::string filepath)
{
    auto dltContent = parseDLTFile(filepath);
    
    std::map<std::string, bool> apids;

    apid->Clear();
    dltentries->DeleteAllItems();
    for(auto it : dltContent){
      long index = dltentries->InsertItem(dltentries->GetItemCount(), std::to_string(it.index).c_str());
      ///dltentries->SetItem(index, 1, it.headerText);
      
      if(!apids[it.apid])
        apid->Append(it.apid);
      apids[it.apid]=true;
      dltentries->SetItem(index, 1, it.timestamp);
      dltentries->SetItem(index, 2, it.apid);
      dltentries->SetItem(index, 3, it.context);
      dltentries->SetItem(index, 4, it.payload);
      
      dltentries->SetItemBackgroundColour(index,getBackgroundByAPIDHardcode(it.apid));
     
     // dltentries->SetItemBackgroundColour(index, 
     // validateExpression(it.payload, ".*hello.*")?
     // wxColour(255, 200, 200):*wxWHITE); 
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
    dltentries->InsertColumn(1, "TIMESTAMP", wxLIST_FORMAT_LEFT, 200);
    dltentries->InsertColumn(2, "APID", wxLIST_FORMAT_LEFT, 100);
    dltentries->InsertColumn(3, "CONTEXT", wxLIST_FORMAT_LEFT, 100);
    dltentries->InsertColumn(4, "payload", wxLIST_FORMAT_LEFT, 600);

    optionPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    openDLTFile = new wxButton(optionPanel, OpenDLTFile, wxT("Open a DLT File(read only, not filtering)"));
   
   
   
    wxBoxSizer *efimerus = new wxBoxSizer(wxHORIZONTAL);
    efimerus->Add(openDLTFile, 0,   wxALL);
    wxArrayString items;
      ///          items.Add("Item 1");
     ///   items.Add("Item 2");
     ///   items.Add("Item 3");
    ///    items.Add("Item 4");
    apid = new wxCheckListBox(optionPanel, wxID_ANY, wxDefaultPosition, wxSize(200,48), items);
    efimerus->Add(apid, 0,   wxALL);
    
    wxButton* filterBySelection = new wxButton(optionPanel, wxID_ANY, wxT("Filter By selection"));
    efimerus->Add(filterBySelection, 0,   wxALL);

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


#if 0 
    void OnGetSelection(wxCommandEvent&) {
        wxArrayInt selections;
        listBox->GetSelections(selections);

        wxString selectedItems;
        for (size_t i = 0; i < selections.size(); ++i) {
            selectedItems += listBox->GetString(selections[i]) + "\n";
        }

        wxMessageBox("Selected Items:\n" + selectedItems, "Selection");
    }
#endif