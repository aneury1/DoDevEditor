// Shared host progress service and compatibility adapters. Plugin-specific
// views live with their plugins.
template <class Fn>
auto networkJob(const wxString& title, Fn fn)
    -> decltype(fn(static_cast<std::atomic<bool>*>(nullptr))) {
  std::atomic<bool> cancel{false};
  auto future = std::async(std::launch::async, [&] { return fn(&cancel); });
  wxProgressDialog progress(title, "Waiting for provider...", 100, this,
                            wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_AUTO_HIDE);
  while (future.wait_for(std::chrono::milliseconds(30)) !=
         std::future_status::ready) {
    if (!progress.Pulse()) cancel = true;
  }
  progress.Update(100);
  return future.get();
}
#include "../../../thirdparty/AIAssistant/src/wx/AiViews.inc"
#include "../../../thirdparty/RemoteJournal/src/wx/JournalLogin.inc"
#include "../../../thirdparty/SerialMonitor/src/wx/SerialView.inc"
