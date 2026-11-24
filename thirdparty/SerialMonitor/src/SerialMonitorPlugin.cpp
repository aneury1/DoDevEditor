#include "dodev/sdk/dodev_plugin_api.h"
#include <string>
namespace {
void openMonitor(const DodevHostAPI* host,void*){
    if(!DODEV_HOST_HAS(host,open_serial_monitor)){
        if(host&&host->log)host->log(host->context,2,"Serial Monitor requires DoDevEditor 3.5 host integration");
        return;
    }
    if(!host->open_serial_monitor(host->context) && host->log)
        host->log(host->context,2,"Could not open Serial Monitor dialog");
}
void view(const DodevHostAPI*,const DodevUiBuilder* ui,void*){
    if(!ui)return;
    ui->label(ui->context,"Serial Monitor | Real-time serial logs and filtering");
    ui->button(ui->context,"Configure / start serial capture...","serialmonitor.open");
    ui->label(ui->context,"Output appears in TASKS next to the integrated terminal.");
}
void shutdown(){}
}
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* host,DodevPluginAPI* plugin){
    if(!host||!plugin||host->api_version!=DODEV_PLUGIN_API_VERSION)return 0;
    if(!DODEV_HOST_HAS(host,open_serial_monitor))return 0;
    if(!host->register_command(host->context,"serialmonitor.open","Monitor serie...",&openMonitor,nullptr))return 0;
    if(!host->register_menu(host->context,"Tools","Monitor serie...","serialmonitor.open"))return 0;
    host->register_tab(host->context,"serialmonitor.help","Serial Monitor",&view,nullptr);
    host->register_setting(host->context,"port","","Serial device path or COM port");
    host->register_setting(host->context,"baud","115200","Baud rate");
    host->register_setting(host->context,"dataBits","8","Data bits: 7 or 8");
    host->register_setting(host->context,"stopBits","1","Stop bits: 1 or 2");
    host->register_setting(host->context,"parity","N","Parity: N, E, O");
    host->register_setting(host->context,"filterMode","all","all | fixed | regex");
    host->register_setting(host->context,"filterText","","Fixed text or regular expression");
    host->register_setting(host->context,"caseSensitive","false","Case-sensitive filter");
    host->register_setting(host->context,"showOnlyMatches","true","Show only matching log lines");
    host->register_setting(host->context,"allowPolkit","false","Request Polkit authorization after permission denied");
    plugin->api_version=DODEV_PLUGIN_API_VERSION;
    if(plugin->struct_size>=offsetof(DodevPluginAPI,shutdown)+sizeof(plugin->shutdown))plugin->shutdown=&shutdown;
    if(host->log)host->log(host->context,1,"Serial Monitor available in Herramientas > Monitor serie...");
    return 1;
}
