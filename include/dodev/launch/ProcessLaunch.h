#pragma once
namespace dodev::launch {
int startEditor(int argc,char** argv,bool direct,int (*runGui)(int,char**)=nullptr);
}
