#include "AiClient.h"
#include <cstdlib>
#include <iostream>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    dodev::ai::Config cfg;cfg.endpoint=argv[1];cfg.model="test-model";cfg.timeoutSeconds=5;
    const auto reply=dodev::ai::complete(cfg,{},"Hello from DoDevEditor","",nullptr);
    if(!reply.success||reply.answer!="mock response"){
        std::cerr<<"AI local HTTP test failed: "<<reply.error<<" / "<<reply.answer<<"\n";return 1;
    }
    auto models=dodev::ai::listModels(cfg);
    if(!models.success||models.models.size()!=2||models.models[0]!="test-code"||models.models[1]!="test-instruct"){
        std::cerr<<"Model discovery failed: "<<models.error<<"\n";return 1;
    }
#ifdef _WIN32
    _putenv_s("DODEV_AI_TEST_TOKEN","safe-local-test-key");
#else
    setenv("DODEV_AI_TEST_TOKEN","safe-local-test-key",1);
#endif
    cfg.authMode="bearer";cfg.keyEnvironment="DODEV_AI_TEST_TOKEN";
    models=dodev::ai::listModels(cfg);
    if(!models.success){std::cerr<<"Authenticated discovery failed: "<<models.error<<"\n";return 1;}
    return 0;
}
