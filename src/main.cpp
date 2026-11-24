#include "EditorApp.h"
#include "dodev/launch/ProcessLaunch.h"
int main(int argc,char** argv) {
    return dodev::launch::startEditor(argc,argv,true,[](int count,char** args){
        dodev::EditorApp app;
        return app.run(count,args);
    });
}
