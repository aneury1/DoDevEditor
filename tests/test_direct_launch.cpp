#include "dodev/launch/ProcessLaunch.h"
#include <iostream>
#include <thread>
#include <chrono>
int main(int argc,char** argv){
    return dodev::launch::startEditor(argc,argv,true,[](int n,char** args){
        for(int i=2;i<n;++i)std::cout<<args[i]<<'\n';
        std::cout<<"GUI callback started"<<std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 17;
    });
}
