#pragma once
#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace dodev::journal {
struct Options {
    std::string host; // SSH hostname/IP, without credentials
    std::string user;
    int sshPort=22;
    std::string unit; // optional systemd unit
    unsigned int lines=200;
    bool follow=true;
    bool sudoNonInteractive=false; // sudo -n only; never request password
};
enum class Filter {All,Fixed,Regex};
struct Snapshot {std::string text,status;std::size_t received=0,dropped=0;bool running=false;};
// Uses the local OpenSSH client without a local shell, in a background thread.
// Only SSH keys already configured by the user are used (BatchMode=yes).
class Session {
public:
    static constexpr std::size_t kMaxLines=20000;
    ~Session(){stop();}
    Session()=default;
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
    static bool validate(const Options&,std::string* error=nullptr);
    static std::vector<std::string> sshArguments(const Options&);
    bool start(const Options&,std::string* error=nullptr);
    void stop();
    bool setFilter(Filter,std::string,bool caseSensitive,std::string* error=nullptr);
    Snapshot snapshot(bool onlyMatching=true,std::size_t maxLines=2500) const;
    std::string exportText(bool onlyMatching=true) const;
    void clear();
    bool running()const{return running_.load();}
private:
    void worker(Options);
    void append(std::string);
    bool matches(const std::string&)const;
    void setStatus(std::string);
    mutable std::mutex mutex_;
    std::thread thread_;
    std::atomic<bool> running_{false},stopping_{false};
    std::atomic<int> processId_{-1};
    std::deque<std::string> lines_;
    std::string fragment_,status_="Idle";
    std::size_t received_=0,dropped_=0;
    Filter filter_=Filter::All;
    std::string pattern_;
    bool caseSensitive_=false;
    std::regex regex_;
};
}
