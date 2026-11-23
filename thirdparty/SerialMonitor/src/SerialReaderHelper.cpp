#include "SerialMonitor.h"
#include <cstdlib>
#include <iostream>
#include <string>
int main(int argc,char** argv){
    if(argc!=6){std::cerr<<"usage: dodev-serial-reader-helper /dev/ttyDEVICE BAUD DATABITS STOPBITS PARITY\n";return 2;}
    dodev::serial::PortOptions options;
    try{options.port=argv[1];options.baud=std::stoi(argv[2]);options.dataBits=std::stoi(argv[3]);options.stopBits=std::stoi(argv[4]);
        options.parity=std::string(argv[5]).size()==1?argv[5][0]:'?';}
    catch(...){std::cerr<<"Invalid serial options\n";return 2;}
    std::string error;const int result=dodev::serial::CaptureSession::runReaderToStdout(options,&error);
    if(result&& !error.empty())std::cerr<<error<<'\n';return result;
}
