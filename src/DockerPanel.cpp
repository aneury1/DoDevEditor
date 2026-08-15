#include "DockerPanel.h"

#include <wx/font.h>
#include <wx/settings.h>
#include <wx/utils.h>

DockerPanel::DockerPanel(wxWindow* parent)
    : wxPanel(parent),
      m_refreshTimer(this)
{
    SetBackgroundColour(wxColour(37, 37, 38));

    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxPanel(this);
    toolbar->SetBackgroundColour(wxColour(37, 37, 38));
    auto* toolbarSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* refreshButton = new wxButton(toolbar, wxID_ANY, "Refresh",
                                       wxDefaultPosition, wxSize(72, 28));
    m_inspectButton = new wxButton(toolbar, wxID_ANY, "Inspect",
                                   wxDefaultPosition, wxSize(70, 28));
    m_logsButton = new wxButton(toolbar, wxID_ANY, "Logs",
                                wxDefaultPosition, wxSize(58, 28));
    m_statsButton = new wxButton(toolbar, wxID_ANY, "Stats",
                                 wxDefaultPosition, wxSize(58, 28));
    m_autoRefresh = new wxCheckBox(toolbar, wxID_ANY, "Auto 5s");
    auto* disableButton = new wxButton(toolbar, wxID_ANY, "Turn off",
                                       wxDefaultPosition, wxSize(72, 28));

    toolbarSizer->Add(refreshButton, 0, wxRIGHT, 4);
    toolbarSizer->Add(m_inspectButton, 0, wxRIGHT, 4);
    toolbarSizer->Add(m_logsButton, 0, wxRIGHT, 4);
    toolbarSizer->Add(m_statsButton, 0, wxRIGHT, 8);
    toolbarSizer->Add(m_autoRefresh, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    toolbarSizer->AddStretchSpacer(1);
    toolbarSizer->Add(disableButton, 0);
    toolbar->SetSizer(toolbarSizer);
    rootSizer->Add(toolbar, 0, wxEXPAND | wxALL, 5);

    m_statusLabel = new wxStaticText(this, wxID_ANY, "Docker inspector is starting...");
    m_statusLabel->SetForegroundColour(wxColour(180, 180, 180));
    rootSizer->Add(m_statusLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 7);

    m_notebook = new wxNotebook(this, wxID_ANY);
    m_notebook->SetBackgroundColour(wxColour(30, 30, 30));

    // Containers page.
    auto* containersPage = new wxPanel(m_notebook);
    containersPage->SetBackgroundColour(wxColour(30, 30, 30));
    auto* containersSizer = new wxBoxSizer(wxVERTICAL);
    m_containersView = new wxDataViewCtrl(containersPage, wxID_ANY,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxDV_ROW_LINES | wxDV_SINGLE | wxBORDER_NONE);
    m_containersView->SetBackgroundColour(wxColour(30, 30, 30));
    m_containersView->SetForegroundColour(wxColour(220, 220, 220));
    m_containersView->SetRowHeight(22);
    m_containersView->AppendTextColumn("Name", 0, wxDATAVIEW_CELL_INERT, 130);
    m_containersView->AppendTextColumn("Image", 1, wxDATAVIEW_CELL_INERT, 150);
    m_containersView->AppendTextColumn("State", 2, wxDATAVIEW_CELL_INERT, 72);
    m_containersView->AppendTextColumn("Status", 3, wxDATAVIEW_CELL_INERT, 170);
    m_containersView->AppendTextColumn("Ports", 4, wxDATAVIEW_CELL_INERT, 190);
    m_containersView->AppendTextColumn("ID", 5, wxDATAVIEW_CELL_INERT, 130);
    m_containersModel = new wxDataViewListStore();
    m_containersView->AssociateModel(m_containersModel);
    m_containersModel->DecRef();
    containersSizer->Add(m_containersView, 1, wxEXPAND);
    containersPage->SetSizer(containersSizer);

    // Images page.
    auto* imagesPage = new wxPanel(m_notebook);
    imagesPage->SetBackgroundColour(wxColour(30, 30, 30));
    auto* imagesSizer = new wxBoxSizer(wxVERTICAL);
    m_imagesView = new wxDataViewCtrl(imagesPage, wxID_ANY,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxDV_ROW_LINES | wxDV_SINGLE | wxBORDER_NONE);
    m_imagesView->SetBackgroundColour(wxColour(30, 30, 30));
    m_imagesView->SetForegroundColour(wxColour(220, 220, 220));
    m_imagesView->SetRowHeight(22);
    m_imagesView->AppendTextColumn("Repository", 0, wxDATAVIEW_CELL_INERT, 170);
    m_imagesView->AppendTextColumn("Tag", 1, wxDATAVIEW_CELL_INERT, 90);
    m_imagesView->AppendTextColumn("Size", 2, wxDATAVIEW_CELL_INERT, 80);
    m_imagesView->AppendTextColumn("Created", 3, wxDATAVIEW_CELL_INERT, 100);
    m_imagesView->AppendTextColumn("ID", 4, wxDATAVIEW_CELL_INERT, 170);
    m_imagesModel = new wxDataViewListStore();
    m_imagesView->AssociateModel(m_imagesModel);
    m_imagesModel->DecRef();
    imagesSizer->Add(m_imagesView, 1, wxEXPAND);
    imagesPage->SetSizer(imagesSizer);

    // Engine page.
    auto* enginePage = new wxPanel(m_notebook);
    enginePage->SetBackgroundColour(wxColour(30, 30, 30));
    auto* engineSizer = new wxBoxSizer(wxVERTICAL);
    m_engineText = new wxTextCtrl(enginePage, wxID_ANY, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    m_engineText->SetBackgroundColour(wxColour(24, 24, 24));
    m_engineText->SetForegroundColour(wxColour(220, 220, 220));
    wxFont monoFont = m_engineText->GetFont();
    monoFont.SetFamily(wxFONTFAMILY_TELETYPE);
    m_engineText->SetFont(monoFont);
    engineSizer->Add(m_engineText, 1, wxEXPAND);
    enginePage->SetSizer(engineSizer);

    // Detail page used by inspect/logs/stats.
    auto* detailPage = new wxPanel(m_notebook);
    detailPage->SetBackgroundColour(wxColour(30, 30, 30));
    auto* detailSizer = new wxBoxSizer(wxVERTICAL);
    m_detailsText = new wxTextCtrl(detailPage, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    m_detailsText->SetBackgroundColour(wxColour(24, 24, 24));
    m_detailsText->SetForegroundColour(wxColour(220, 220, 220));
    m_detailsText->SetFont(monoFont);
    detailSizer->Add(m_detailsText, 1, wxEXPAND);
    detailPage->SetSizer(detailSizer);

    m_notebook->AddPage(containersPage, "Containers", true);
    m_notebook->AddPage(imagesPage, "Images", false);
    m_notebook->AddPage(enginePage, "Engine", false);
    m_notebook->AddPage(detailPage, "Details", false);
    rootSizer->Add(m_notebook, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    SetSizer(rootSizer);

    refreshButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshDocker(); });
    m_inspectButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { InspectSelected(); });
    m_logsButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowSelectedLogs(); });
    m_statsButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowSelectedStats(); });
    disableButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        if (m_disableCallback)
            m_disableCallback();
    });

    m_autoRefresh->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& event)
    {
        if (event.IsChecked())
            m_refreshTimer.Start(5000);
        else
            m_refreshTimer.Stop();
    });

    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { RefreshDocker(); });

    m_containersView->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent& event)
    {
        SelectContainer(event.GetItem());
    });
    m_imagesView->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent& event)
    {
        SelectImage(event.GetItem());
    });
    m_containersView->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent& event)
    {
        SelectContainer(event.GetItem());
        InspectSelected();
    });
    m_imagesView->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent& event)
    {
        SelectImage(event.GetItem());
        InspectSelected();
    });

    UpdateActionState();
    RefreshDocker();
}

DockerPanel::~DockerPanel()
{
    if (m_refreshTimer.IsRunning())
        m_refreshTimer.Stop();
}

void DockerPanel::SetDisableCallback(std::function<void()> callback)
{
    m_disableCallback = std::move(callback);
}

wxString DockerPanel::Quote(const wxString& value)
{
#ifdef __WXMSW__
    wxString escaped = value;
    escaped.Replace("\"", "\\\"");
    return "\"" + escaped + "\"";
#else
    wxString escaped = value;
    escaped.Replace("'", "'\"'\"'");
    return "'" + escaped + "'";
#endif
}

bool DockerPanel::RunDocker(const wxString& args,
                            wxArrayString& output,
                            wxArrayString& errors,
                            long* exitCode) const
{
    output.Clear();
    errors.Clear();
    const wxString command = "docker " + args;
    const long code = wxExecute(command, output, errors, wxEXEC_SYNC);
    if (exitCode)
        *exitCode = code;
    return code == 0;
}

wxString DockerPanel::JoinLines(const wxArrayString& lines)
{
    wxString text;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        text += lines[i];
        if (i + 1 < lines.size())
            text += "\n";
    }
    return text;
}

wxString DockerPanel::Field(const wxArrayString& fields, size_t index)
{
    if (index < fields.size())
        return fields[index];

    return wxString();
}

bool DockerPanel::CheckDockerAvailable()
{
    wxArrayString output;
    wxArrayString errors;
    long code = -1;
    if (!RunDocker("version --format \"{{.Server.Version}}\"", output, errors, &code))
    {
        wxString reason = JoinLines(errors);
        if (reason.IsEmpty())
            reason = "Docker CLI not found or Docker daemon is unavailable.";
        m_statusLabel->SetLabel("Docker unavailable: " + reason.BeforeFirst('\n'));
        m_statusLabel->SetForegroundColour(wxColour(230, 120, 120));
        m_engineText->SetValue("Docker command failed.\n\n" + reason);
        return false;
    }

    wxArrayString contextOutput;
    wxArrayString contextErrors;
    RunDocker("context show", contextOutput, contextErrors);

    wxString serverVersion("unknown");
    if (!output.IsEmpty())
        serverVersion = output[0];

    wxString context("default");
    if (!contextOutput.IsEmpty())
        context = contextOutput[0];
    m_statusLabel->SetLabel("Docker connected  |  context: " + context +
                            "  |  engine: " + serverVersion);
    m_statusLabel->SetForegroundColour(wxColour(120, 200, 140));
    return true;
}

void DockerPanel::RefreshDocker()
{
    m_containers.clear();
    m_images.clear();
    if (m_containersModel)
        m_containersModel->DeleteAllItems();
    if (m_imagesModel)
        m_imagesModel->DeleteAllItems();

    m_selectionType = SelectionType::None;
    m_selectedRow = wxNOT_FOUND;
    UpdateActionState();

    if (!CheckDockerAvailable())
        return;

    LoadContainers();
    LoadImages();
    LoadEngineInfo();
}

void DockerPanel::LoadContainers()
{
    wxArrayString output;
    wxArrayString errors;
    const wxString format =
        "container ls -a --no-trunc --format \"{{.ID}}\\t{{.Names}}\\t{{.Image}}\\t{{.State}}\\t{{.Status}}\\t{{.Ports}}\"";
    if (!RunDocker(format, output, errors))
        return;

    for (const wxString& line : output)
    {
        wxArrayString fields = wxSplit(line, '\t');
        if (fields.size() < 5)
            continue;

        DockerContainerInfo info;
        info.id = Field(fields, 0);
        info.name = Field(fields, 1);
        info.image = Field(fields, 2);
        info.state = Field(fields, 3);
        info.status = Field(fields, 4);
        info.ports = Field(fields, 5);
        m_containers.push_back(info);

        wxVector<wxVariant> row;
        row.push_back(info.name);
        row.push_back(info.image);
        row.push_back(info.state);
        row.push_back(info.status);
        row.push_back(info.ports);
        row.push_back(info.id);
        m_containersModel->AppendItem(row);
    }
}

void DockerPanel::LoadImages()
{
    wxArrayString output;
    wxArrayString errors;
    const wxString format =
        "image ls --no-trunc --format \"{{.Repository}}\\t{{.Tag}}\\t{{.ID}}\\t{{.Size}}\\t{{.CreatedSince}}\"";
    if (!RunDocker(format, output, errors))
        return;

    for (const wxString& line : output)
    {
        wxArrayString fields = wxSplit(line, '\t');
        if (fields.size() < 4)
            continue;

        DockerImageInfo info;
        info.repository = Field(fields, 0);
        info.tag = Field(fields, 1);
        info.id = Field(fields, 2);
        info.size = Field(fields, 3);
        info.created = Field(fields, 4);
        m_images.push_back(info);

        wxVector<wxVariant> row;
        row.push_back(info.repository);
        row.push_back(info.tag);
        row.push_back(info.size);
        row.push_back(info.created);
        row.push_back(info.id);
        m_imagesModel->AppendItem(row);
    }
}

void DockerPanel::LoadEngineInfo()
{
    wxArrayString versionOutput;
    wxArrayString versionErrors;
    wxArrayString infoOutput;
    wxArrayString infoErrors;

    RunDocker("version", versionOutput, versionErrors);
    RunDocker("info", infoOutput, infoErrors);

    wxString text = "DOCKER VERSION\n==============\n";
    text += JoinLines(versionOutput);
    if (!versionErrors.IsEmpty())
        text += "\n" + JoinLines(versionErrors);
    text += "\n\nDOCKER INFO\n===========\n";
    text += JoinLines(infoOutput);
    if (!infoErrors.IsEmpty())
        text += "\n" + JoinLines(infoErrors);

    m_engineText->SetValue(text);
}

void DockerPanel::SelectContainer(const wxDataViewItem& item)
{
    if (!item.IsOk() || !m_containersModel)
    {
        m_selectionType = SelectionType::None;
        m_selectedRow = wxNOT_FOUND;
        UpdateActionState();
        return;
    }

    const unsigned int row = m_containersModel->GetRow(item);
    if (row >= m_containers.size())
        return;

    m_selectionType = SelectionType::Container;
    m_selectedRow = static_cast<int>(row);
    UpdateActionState();
}

void DockerPanel::SelectImage(const wxDataViewItem& item)
{
    if (!item.IsOk() || !m_imagesModel)
    {
        m_selectionType = SelectionType::None;
        m_selectedRow = wxNOT_FOUND;
        UpdateActionState();
        return;
    }

    const unsigned int row = m_imagesModel->GetRow(item);
    if (row >= m_images.size())
        return;

    m_selectionType = SelectionType::Image;
    m_selectedRow = static_cast<int>(row);
    UpdateActionState();
}

wxString DockerPanel::SelectedObjectId() const
{
    if (m_selectedRow < 0)
        return wxEmptyString;

    if (m_selectionType == SelectionType::Container &&
        static_cast<size_t>(m_selectedRow) < m_containers.size())
    {
        return m_containers[static_cast<size_t>(m_selectedRow)].id;
    }

    if (m_selectionType == SelectionType::Image &&
        static_cast<size_t>(m_selectedRow) < m_images.size())
    {
        return m_images[static_cast<size_t>(m_selectedRow)].id;
    }

    return wxEmptyString;
}

void DockerPanel::UpdateActionState()
{
    const bool hasSelection = m_selectionType != SelectionType::None &&
                              !SelectedObjectId().IsEmpty();
    const bool containerSelected = m_selectionType == SelectionType::Container && hasSelection;

    if (m_inspectButton)
        m_inspectButton->Enable(hasSelection);
    if (m_logsButton)
        m_logsButton->Enable(containerSelected);
    if (m_statsButton)
        m_statsButton->Enable(containerSelected);
}

void DockerPanel::ShowDetails(const wxString& title,
                              const wxArrayString& output,
                              const wxArrayString& errors,
                              long exitCode)
{
    wxString text = title + "\n";
    text += "==============================\n\n";
    if (!output.IsEmpty())
        text += JoinLines(output);
    if (!errors.IsEmpty())
    {
        if (!output.IsEmpty())
            text += "\n\n";
        text += "stderr:\n" + JoinLines(errors);
    }
    if (exitCode != 0)
        text += wxString::Format("\n\nExit code: %ld", exitCode);

    m_detailsText->SetValue(text);
    m_notebook->SetSelection(3);
}

void DockerPanel::InspectSelected()
{
    const wxString id = SelectedObjectId();
    if (id.IsEmpty())
        return;

    wxArrayString output;
    wxArrayString errors;
    long code = -1;
    RunDocker("inspect " + Quote(id), output, errors, &code);
    ShowDetails("docker inspect " + id, output, errors, code);
}

void DockerPanel::ShowSelectedLogs()
{
    if (m_selectionType != SelectionType::Container)
        return;
    const wxString id = SelectedObjectId();
    if (id.IsEmpty())
        return;

    wxArrayString output;
    wxArrayString errors;
    long code = -1;
    RunDocker("logs --tail 300 --timestamps " + Quote(id), output, errors, &code);
    ShowDetails("docker logs " + id, output, errors, code);
}

void DockerPanel::ShowSelectedStats()
{
    if (m_selectionType != SelectionType::Container)
        return;
    const wxString id = SelectedObjectId();
    if (id.IsEmpty())
        return;

    wxArrayString output;
    wxArrayString errors;
    long code = -1;
    RunDocker("stats --no-stream " + Quote(id), output, errors, &code);
    ShowDetails("docker stats " + id, output, errors, code);
}
