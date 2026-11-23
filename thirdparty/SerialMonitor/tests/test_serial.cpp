#include "SerialMonitor.h"
#include <stdexcept>
#define REQUIRE(cond) do { if(!(cond)) throw std::runtime_error(#cond); } while(0)
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#ifndef _WIN32
#include <pty.h>
#include <unistd.h>
#endif
int main(){
    using namespace dodev::serial;
    CaptureSession m;std::string error;
    REQUIRE(!m.setFilter(FilterMode::Regex,"[",false,&error));
    REQUIRE(error.find("Invalid")!=std::string::npos);
    REQUIRE(m.setFilter(FilterMode::Fixed,"error",false,&error));
    PortOptions bad;bad.port="/tmp/serial-unsafe";bad.baud=115200;
#ifndef _WIN32
    REQUIRE(!CaptureSession::validatePort("/tmp/serial-unsafe",&error));
    int master=-1,slave=-1;
    REQUIRE(openpty(&master,&slave,nullptr,nullptr,nullptr)==0);
    const char* port=ttyname(slave);REQUIRE(port);
    PortOptions options;options.port=port;options.baud=115200;
    PortOptions forbidden;forbidden.port="/dev/null";
    REQUIRE(CaptureSession::runReaderToStdout(forbidden,&error)==2);
    REQUIRE(m.start(options,&error));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    const std::string lines="hello device\r\nERROR one\r\ntext two\r\nError 3\r\n";
    REQUIRE(write(master,lines.data(),lines.size())==(ssize_t)lines.size());
    bool ready=false;
    for(int i=0;i<40;++i){std::this_thread::sleep_for(std::chrono::milliseconds(30));
        if(m.snapshot(false).received>=4){ready=true;break;}}
    REQUIRE(ready);
    const auto a=m.snapshot(true).output;
    REQUIRE(a.find("ERROR one")!=std::string::npos);
    REQUIRE(a.find("Error 3")!=std::string::npos);
    REQUIRE(a.find("hello")==std::string::npos);
    REQUIRE(m.setFilter(FilterMode::Regex,"^ERROR\\s+one$",true,&error));
    const auto b=m.snapshot(true).output;
    REQUIRE(b.find("ERROR one")!=std::string::npos);
    REQUIRE(b.find("Error 3")==std::string::npos);
    REQUIRE(m.setFilter(FilterMode::All,"",false,&error));
    REQUIRE(m.exportText().find("hello device")!=std::string::npos);
    m.stop();close(master);close(slave);
    REQUIRE(!m.running());
#else
    REQUIRE(!CaptureSession::validateOptions(bad,&error) || !error.empty());
#endif
    std::cout<<"serial filter, session and PTY checks passed\n";
}
