#ifndef DATA_SERVER_EDITOR_H_DEFINED
#define DATA_SERVER_EDITOR_H_DEFINED
#include <wx/wx.h>
#include "EditorTab.h"
#include "TabContainer.h"

struct DataServerProtocols{
    virtual ~DataServerProtocols(){}
};

struct ReceivedProtocols : public DataServerProtocols{
    virtual ~ReceivedProtocols(){}
    virtual Response Start() = 0;
    virtual Response Stop()  = 0;
    virtual Response OnConnection(std::function<void()> fn)=0;
    virtual Response OnDisconnection(std::function<void()> fn)=0;
    virtual Response OnError() = 0;
    virtual wxPanel *GetPanel() = 0;
};


enum class SupportedProtocolType{
   HttpServer,
   HttpClient,
   FileServer,
   FileClient,
   ReadOnlyData,
   SendPayload
};


struct DataServerEditor : public EditorTab{
    TabContainer *tabcontainer;
    DataServerProtocols *currentProtocol;
    wxBoxSizer *mainSizer;
    wxPanel *actionSelectorPanel;
    wxBoxSizer *actionPanelSizer;

    wxStaticText *labelProtocolSelection;
    wxComboBox *protocols;
    wxStaticText *labelIpAddress;
    wxTextCtrl   *ipOurl;
    wxStaticText *labelPort;
    wxTextCtrl   *port;
    wxButton     *start;
    wxButton     *stop;
    
    void setUpTopController();


    DataServerEditor(wxWindow *parent);
   
    virtual ~DataServerEditor(){}

    virtual int getTypeOfEditor() ;
    
    virtual std::vector<uint8_t> getData() ;

    virtual Response setData(std::vector<uint8_t> d);

    virtual bool wasChanged() ;

    virtual Response saveDocument() ;

    virtual Response openFile(std::string filepath) ;

};



#endif 