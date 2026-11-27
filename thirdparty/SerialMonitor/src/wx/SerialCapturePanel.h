#pragma once
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <filesystem>
#include <fstream>

#include "SerialMonitor.h"
namespace wxui {
class SerialCapturePanel final : public wxPanel {
  dodev::serial::CaptureSession session_;
  wxTimer timer_;
  std::filesystem::path root_;
  wxTextCtrl *output_, *filter_, *send_;
  wxStaticText* status_;
  wxCheckBox *regex_, *sensitive_, *matched_;
  std::string last_;
  static wxString w(const std::string& s) {
    auto r = wxString::FromUTF8(s.data(), s.size());
    return r.empty() && !s.empty() ? wxString::From8BitData(s.data(), s.size())
                                   : r;
  }
  static std::string s(const wxString& v) {
    auto b = v.ToUTF8();
    return b.data() ? std::string(b.data(), b.length()) : std::string();
  }
  void poll() {
    auto v = session_.snapshot(matched_->GetValue());
    status_->SetLabel(w(v.status));
    if (last_ != v.output) {
      last_ = v.output;
      output_->ChangeValue(w(last_));
      output_->SetInsertionPointEnd();
    }
  }
  bool apply(std::string& err) {
    return session_.setFilter(
        regex_->GetValue() ? dodev::serial::FilterMode::Regex
                           : dodev::serial::FilterMode::Fixed,
        s(filter_->GetValue()), sensitive_->GetValue(), &err);
  }
  void clipboard(const wxString& value) {
    if (wxTheClipboard->Open()) {
      wxTheClipboard->SetData(new wxTextDataObject(value));
      wxTheClipboard->Close();
    }
  }

 public:
  SerialCapturePanel(wxWindow* parent, std::filesystem::path root)
      : wxPanel(parent), timer_(this), root_(std::move(root)) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    SetSizer(layout);
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    layout->Add(actions, 0, wxEXPAND);
    auto button = [&](const wxString& text, auto fn) {
      auto* b = new wxButton(this, wxID_ANY, text);
      actions->Add(b, 0, wxALL, 2);
      b->Bind(wxEVT_BUTTON, [fn](wxCommandEvent&) { fn(); });
    };
    button("Disconnect", [this] {
      session_.stop();
      poll();
    });
    button("Copy selection",
           [this] { clipboard(output_->GetStringSelection()); });
    button("Copy logs",
           [this] { clipboard(w(session_.exportText(matched_->GetValue()))); });
    button("Clear", [this] {
      session_.clear();
      poll();
    });
    button("Export...", [this] {
      wxFileDialog d(this, "Export serial log", w(root_.u8string()),
                     "serial.txt", "Text|*.txt",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
      if (d.ShowModal() == wxID_OK) {
        std::ofstream f(std::filesystem::u8path(s(d.GetPath())),
                        std::ios::binary);
        f << session_.exportText(matched_->GetValue());
        if (!f) wxLogError("Serial export failed");
      }
    });
    filter_ = new wxTextCtrl(this, wxID_ANY);
    filter_->SetHint("Payload / regular expression");
    layout->Add(filter_, 0, wxEXPAND | wxALL, 3);
    auto* flags = new wxBoxSizer(wxHORIZONTAL);
    layout->Add(flags, 0, wxEXPAND);
    regex_ = new wxCheckBox(this, wxID_ANY, "Regex");
    sensitive_ = new wxCheckBox(this, wxID_ANY, "Case sensitive");
    matched_ = new wxCheckBox(this, wxID_ANY, "Only matches");
    for (auto* f : {regex_, sensitive_, matched_}) flags->Add(f, 0, wxALL, 3);
    auto filter = [this](wxCommandEvent&) {
      std::string err;
      if (!apply(err))
        status_->SetLabel(w(err));
      else
        poll();
    };
    filter_->Bind(wxEVT_TEXT, filter);
    regex_->Bind(wxEVT_CHECKBOX, filter);
    sensitive_->Bind(wxEVT_CHECKBOX, filter);
    matched_->Bind(wxEVT_CHECKBOX, filter);
    status_ = new wxStaticText(this, wxID_ANY, "Connecting...");
    layout->Add(status_, 0, wxALL, 3);
    output_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                             wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
    layout->Add(output_, 1, wxEXPAND | wxALL, 3);
    send_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                           wxTE_PROCESS_ENTER);
    send_->SetHint("Send bytes (Enter)");
    layout->Add(send_, 0, wxEXPAND | wxALL, 3);
    send_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) {
      std::string err;
      if (!session_.sendBytes(s(send_->GetValue()) + "\r\n", &err))
        wxLogError("%s", w(err));
      else
        send_->Clear();
    });
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { poll(); }, timer_.GetId());
  }
  ~SerialCapturePanel() override {
    timer_.Stop();
    session_.stop();
  }
  bool start(const dodev::serial::PortOptions& o, const std::string& mode,
             const std::string& text, bool sensitive, bool matched,
             std::string& error) {
    regex_->SetValue(mode == "regex");
    sensitive_->SetValue(sensitive);
    matched_->SetValue(matched);
    filter_->ChangeValue(w(mode == "all" ? "" : text));
    if (!apply(error) || !session_.start(o, &error)) return false;
    timer_.Start(150);
    return true;
  }
};
}  // namespace wxui
