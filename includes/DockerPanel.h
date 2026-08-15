#ifndef DOCKER_PANEL_H
#define DOCKER_PANEL_H

#include <wx/wx.h>
#include <wx/dataview.h>
#include <wx/notebook.h>
#include <wx/timer.h>

#include <functional>
#include <vector>

struct DockerContainerInfo
{
    wxString id;
    wxString name;
    wxString image;
    wxString state;
    wxString status;
    wxString ports;
};

struct DockerImageInfo
{
    wxString repository;
    wxString tag;
    wxString id;
    wxString size;
    wxString created;
};

class DockerPanel : public wxPanel
{
public:
    explicit DockerPanel(wxWindow* parent);
    ~DockerPanel() override;

    void RefreshDocker();
    void SetDisableCallback(std::function<void()> callback);

private:
    enum class SelectionType
    {
        None,
        Container,
        Image
    };

    wxStaticText* m_statusLabel = nullptr;
    wxButton* m_inspectButton = nullptr;
    wxButton* m_logsButton = nullptr;
    wxButton* m_statsButton = nullptr;
    wxCheckBox* m_autoRefresh = nullptr;

    wxNotebook* m_notebook = nullptr;

    wxDataViewCtrl* m_containersView = nullptr;
    wxDataViewListStore* m_containersModel = nullptr;

    wxDataViewCtrl* m_imagesView = nullptr;
    wxDataViewListStore* m_imagesModel = nullptr;

    wxTextCtrl* m_engineText = nullptr;
    wxTextCtrl* m_detailsText = nullptr;

    wxTimer m_refreshTimer;
    std::vector<DockerContainerInfo> m_containers;
    std::vector<DockerImageInfo> m_images;

    SelectionType m_selectionType = SelectionType::None;
    int m_selectedRow = wxNOT_FOUND;
    std::function<void()> m_disableCallback;

    bool RunDocker(const wxString& args,
                   wxArrayString& output,
                   wxArrayString& errors,
                   long* exitCode = nullptr) const;
    static wxString JoinLines(const wxArrayString& lines);
    static wxString Quote(const wxString& value);
    static wxString Field(const wxArrayString& fields, size_t index);

    void LoadContainers();
    void LoadImages();
    void LoadEngineInfo();
    bool CheckDockerAvailable();

    void SelectContainer(const wxDataViewItem& item);
    void SelectImage(const wxDataViewItem& item);
    wxString SelectedObjectId() const;
    void UpdateActionState();

    void InspectSelected();
    void ShowSelectedLogs();
    void ShowSelectedStats();
    void ShowDetails(const wxString& title,
                     const wxArrayString& output,
                     const wxArrayString& errors,
                     long exitCode);
};

#endif // DOCKER_PANEL_H
