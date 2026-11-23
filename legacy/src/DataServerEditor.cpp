#include "DataServerEditor.h"
#include "utils.h"
#include <iostream>
#include <wx/listctrl.h>
#include <functional>

namespace {


struct HttpServer : public ReceivedProtocols{
    short port; 
    ///std::string ipAddressOrURL;
    HttpServer(short p): port{p}{}
    virtual Response Start(){}
    virtual Response Stop(){}
    virtual Response OnConnection(std::function<void()> fn){}
    virtual Response OnDisconnection(std::function<void()> fn){}
    virtual Response OnError(){}
    wxPanel *GetPanel(){}
};


struct FileServer : public ReceivedProtocols{
    short port; 
    ///std::string ipAddressOrURL;
    FileServer(short p): port{p}{}
    virtual Response Start(){}
    virtual Response Stop(){}
    virtual Response OnConnection(std::function<void()> fn){}
    virtual Response OnDisconnection(std::function<void()> fn){}
    virtual Response OnError(){}
    virtual wxPanel *GetPanel(){}
};
struct FileClient : public ReceivedProtocols{
    
    wxArrayString files;
    wxPanel *panel;
    wxBoxSizer *sizer;
    wxListCtrl *elements;

    void setUpUi(wxWindow *parent){

    }

    short port; 
    ///std::string ipAddressOrURL;
    FileClient(wxWindow *parent,short p): port{p}{
        setUpUi(parent);
    }
    virtual Response Start(){}
    virtual Response Stop(){}
    virtual Response OnConnection(std::function<void()> fn){}
    virtual Response OnDisconnection(std::function<void()> fn){}
    virtual Response OnError(){}
    virtual wxPanel *GetPanel(){
        return panel;
    }
};

}



    
void DataServerEditor::setUpTopController(){

    auto a = GetAllLocalIPs(); 

    for(auto i : a )
      std::cout << i.ToStdString()<<"\n";

    auto parent = this;
    actionSelectorPanel = new wxPanel(this, wxID_ANY,wxDefaultPosition,wxDefaultSize);
    labelProtocolSelection=new wxStaticText(actionSelectorPanel,wxID_ANY, wxT("Select the protocol"));
    wxArrayString  arrProtocols;
    arrProtocols.Add(wxT("Http Server"));
    arrProtocols.Add(wxT("Http Client"));
    arrProtocols.Add(wxT("Http File Server"));
    arrProtocols.Add(wxT("Http File Client"));
    arrProtocols.Add(wxT("Http Read Only Data"));
    arrProtocols.Add(wxT("Http File Send Payload"));
    
   
    auto ip = GetAllLocalIPs();
    wxArrayString ipArr;
    for(auto it : ip){
        ipArr.Add(it);
    } 
    ipArr.Add("127.0.0.1");
    ipArr.Add("localhost");

    protocols = new wxComboBox(actionSelectorPanel, wxID_ANY, "Select Protocol",
    wxDefaultPosition, wxDefaultSize, arrProtocols, wxCB_DROPDOWN);
    labelIpAddress = new wxStaticText(actionSelectorPanel, wxID_ANY, wxT("Enter IP:"));
    ipOurl= new wxTextCtrl(actionSelectorPanel, wxID_ANY);
    ipOurl->AutoComplete(ipArr);
    labelPort= new wxStaticText(actionSelectorPanel, wxID_ANY, wxT("Enter Port:"));;
    port= new wxTextCtrl(actionSelectorPanel, wxID_ANY);
    start = new wxButton(actionSelectorPanel, wxID_ANY, wxT("Start"));
    stop = new wxButton(actionSelectorPanel, wxID_ANY, wxT("Stop"));

    actionPanelSizer = new wxBoxSizer(wxHORIZONTAL);

    actionPanelSizer->Add(labelProtocolSelection, 0,wxALIGN_CENTRE_VERTICAL|wxALL, 1);
    actionPanelSizer->Add(protocols, 0,wxEXPAND|wxALL, 1);
    actionPanelSizer->Add(labelIpAddress, 0,wxALIGN_CENTRE_VERTICAL|wxALL, 1);
    actionPanelSizer->Add(ipOurl, 0,wxEXPAND|wxALL, 1);
    actionPanelSizer->Add(labelPort, 0,wxALIGN_CENTRE_VERTICAL|wxALL, 1);
    actionPanelSizer->Add(port, 0,wxEXPAND|wxALL, 1);
    actionPanelSizer->Add(start, 0,wxEXPAND|wxALL, 1);
    actionPanelSizer->Add(stop, 0,wxEXPAND|wxALL, 1);

    actionSelectorPanel->SetSizer(actionPanelSizer);

    mainSizer->Add(actionSelectorPanel, 0, wxEXPAND);
}





DataServerEditor::DataServerEditor(wxWindow *parent): EditorTab(parent){
    tabcontainer = (TabContainer *)parent;
    mainSizer = new wxBoxSizer(wxVERTICAL);
    setUpTopController();
    SetSizer(mainSizer);
}

int DataServerEditor::getTypeOfEditor(){
    return 0;
}
    
std::vector<uint8_t> DataServerEditor::getData(){
    return {};
}

Response DataServerEditor::setData(std::vector<uint8_t> d){
   
    ///Depends On protocol handler send or Wait for data.
    return Response::Success;
}

bool DataServerEditor::wasChanged(){
    return false;
}

Response DataServerEditor::saveDocument(){
    return Response::Success;
}

Response DataServerEditor::openFile(std::string filepath){
    return Response::Success;
}