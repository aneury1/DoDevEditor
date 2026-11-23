#include "EditorApp.h"
#include "EmbeddedSerialUi.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace dodev {
namespace {
Rid widget(Rid parent,const char* id){return parent==R_INVALID_ID?R_INVALID_ID:Rwidget_find(parent,id);}
std::string input(Rid window,const char* name){
    auto id=widget(window,name);if(id==R_INVALID_ID)return {};
    const char* value=Rtext_input_get_text(id);return value?value:"";
}
void setInput(Rid window,const char* name,const std::string& value){auto id=widget(window,name);if(id!=R_INVALID_ID)Rtext_input_set_text(id,value.c_str());}
bool isChecked(Rid window,const char* name){auto id=widget(window,name);return id!=R_INVALID_ID&&Rcheckbox_is_checked(id)!=R_FALSE;}
void setChecked(Rid window,const char* name,bool selected){auto id=widget(window,name);if(id!=R_INVALID_ID)Rcheckbox_set_checked(id,selected?R_TRUE:R_FALSE);}
void label(Rid window,const char* name,const std::string& value){auto id=widget(window,name);if(id!=R_INVALID_ID)Rlabel_set_text(id,value.c_str());}
void add(Rid window,const char* name,std::initializer_list<const char*> values){auto combo=widget(window,name);if(combo==R_INVALID_ID)return;
    for(const auto* value:values)Rcombo_box_add_item(combo,value);
}
std::string selected(Rid window,const char* name){auto combo=widget(window,name);if(combo==R_INVALID_ID)return {};
    const int ix=Rcombo_box_get_selected(combo);if(ix<0)return {};
    const char* value=Rcombo_box_get_item_text(combo,ix);return value?value:"";
}
void selectString(Rid window,const char* name,const std::string& target){auto combo=widget(window,name);if(combo==R_INVALID_ID)return;
    for(int i=0;i<Rcombo_box_get_item_count(combo);++i){const char* item=Rcombo_box_get_item_text(combo,i);
        if(item && target==item){Rcombo_box_set_selected(combo,i);return;}}
    if(Rcombo_box_get_item_count(combo)>0)Rcombo_box_set_selected(combo,0);
}
bool truthy(const std::string& text){return text=="true"||text=="1"||text=="yes";}
std::string config(const PluginManager& mgr,const char* name,const char* fallback){
    const auto value=mgr.pluginSetting("com.dodev.serialmonitor",name);return value.empty()?fallback:value;
}
void addButton(Rid parent,const char* title,const char* command){
    auto button=Rbutton_create(title);Rwidget_set_class(button,"chip");Rwidget_set_action(button,command);Rwidget_add_child(parent,button);
}
}

bool EditorApp::openSerialMonitor(){
    if(serialWindow_!=R_INVALID_ID&&Rwindow_is_valid(serialWindow_)){Rwindow_show(serialWindow_);return true;}
    serialWindow_=Rwindow_create_from_xml(ui::kSerialMonitorWindow.data(),ui::kSerialMonitorWindow.size());
    if(serialWindow_==R_INVALID_ID)return false;
    add(serialWindow_,"serial_baud",{"1200","2400","4800","9600","19200","38400","57600","115200","230400","460800"});
    add(serialWindow_,"serial_data_bits",{"8","7"});
    add(serialWindow_,"serial_stop_bits",{"1","2"});
    add(serialWindow_,"serial_parity",{"N","E","O"});
    add(serialWindow_,"serial_filter_mode",{"All","Fixed text","Regex"});
    std::string port=config(pluginManager_,"port","");
#ifdef _WIN32
    if(port.empty())port="COM3";
#else
    if(port.empty())port="/dev/ttyUSB0";
#endif
    setInput(serialWindow_,"serial_port",port);
    selectString(serialWindow_,"serial_baud",config(pluginManager_,"baud","115200"));
    selectString(serialWindow_,"serial_data_bits",config(pluginManager_,"dataBits","8"));
    selectString(serialWindow_,"serial_stop_bits",config(pluginManager_,"stopBits","1"));
    selectString(serialWindow_,"serial_parity",config(pluginManager_,"parity","N"));
    const auto filter=config(pluginManager_,"filterMode","all");
    selectString(serialWindow_,"serial_filter_mode",filter=="regex"?"Regex":filter=="fixed"?"Fixed text":"All");
    setInput(serialWindow_,"serial_filter_text",config(pluginManager_,"filterText",""));
    setChecked(serialWindow_,"serial_case_sensitive",truthy(config(pluginManager_,"caseSensitive","false")));
    serialOnlyMatches_=truthy(config(pluginManager_,"showOnlyMatches","true"));
    setChecked(serialWindow_,"serial_only_matches",serialOnlyMatches_);
    setChecked(serialWindow_,"serial_polkit",truthy(config(pluginManager_,"allowPolkit","false")));
    Rcombo_box_set_changed_callback(widget(serialWindow_,"serial_ports_list"),&EditorApp::serialPortSelected,this);
    label(serialWindow_,"serial_status",serialCapture_.snapshot().status);
    refreshSerialPortChoices();
    Rwindow_show(serialWindow_);
    std::clog<<"[DoDevEditor][Serial] Serial Monitor dialog opened\n";
    return true;
}
void EditorApp::refreshSerialPortChoices(){
    if(serialWindow_==R_INVALID_ID||!Rwindow_is_valid(serialWindow_))return;
    const auto ports=serial::CaptureSession::availablePorts();
    auto combo=widget(serialWindow_,"serial_ports_list");
    Rcombo_box_clear_items(combo);
    Rcombo_box_add_item(combo,"Select device...");
    for(const auto& port:ports)Rcombo_box_add_item(combo,port.c_str());
    Rcombo_box_set_selected(combo,0);
    label(serialWindow_,"serial_detected",std::to_string(ports.size())+" serial device(s) found. You can type a path or select a detected device.");
}
void EditorApp::serialPortSelected(Rid,int index,void* ctx){
    auto& app=*static_cast<EditorApp*>(ctx);
    if(index<=0||app.serialWindow_==R_INVALID_ID)return;
    const auto* value=Rcombo_box_get_item_text(widget(app.serialWindow_,"serial_ports_list"),index);
    if(value)setInput(app.serialWindow_,"serial_port",value);
}
bool EditorApp::serialApplyFilter(){
    if(serialWindow_==R_INVALID_ID||!Rwindow_is_valid(serialWindow_))return false;
    const auto mode=selected(serialWindow_,"serial_filter_mode");
    const auto filterMode=mode=="Regex"?serial::FilterMode::Regex:mode=="Fixed text"?serial::FilterMode::Fixed:serial::FilterMode::All;
    const auto pattern=input(serialWindow_,"serial_filter_text");
    const bool sensitive=isChecked(serialWindow_,"serial_case_sensitive");
    std::string error;
    if(!serialCapture_.setFilter(filterMode,pattern,sensitive,&error)){
        label(serialWindow_,"serial_status",error);setStatus(error);return false;
    }
    serialOnlyMatches_=isChecked(serialWindow_,"serial_only_matches");
    pluginManager_.updatePluginSetting("com.dodev.serialmonitor","filterMode",filterMode==serial::FilterMode::Regex?"regex":filterMode==serial::FilterMode::Fixed?"fixed":"all");
    pluginManager_.updatePluginSetting("com.dodev.serialmonitor","filterText",pattern);
    pluginManager_.updatePluginSetting("com.dodev.serialmonitor","caseSensitive",sensitive?"true":"false");
    pluginManager_.updatePluginSetting("com.dodev.serialmonitor","showOnlyMatches",serialOnlyMatches_?"true":"false");
    if(serialTaskFilterInput_!=R_INVALID_ID&&Rwidget_is_valid(serialTaskFilterInput_))Rtext_input_set_text(serialTaskFilterInput_,pattern.c_str());
    if(serialTaskFilterMode_!=R_INVALID_ID&&Rwidget_is_valid(serialTaskFilterMode_))
        Rcombo_box_set_selected(serialTaskFilterMode_,filterMode==serial::FilterMode::Regex?2:filterMode==serial::FilterMode::Fixed?1:0);
    serialLastText_.clear();refreshSerialTasks();
    return true;
}
void EditorApp::openSerialTasks(){
    if(serialTaskPage_==R_INVALID_ID||!Rwidget_is_valid(serialTaskPage_)){
        serialTaskPage_=Rwidget_create(R_WIDGET_VBOX);
        Rwidget_set_class(serialTaskPage_,"music-surface surface");
        auto header=Rwidget_create(R_WIDGET_HBOX);Rwidget_set_class(header,"bar");
        Rwidget_add_child(serialTaskPage_,header);
        auto title=Rlabel_create("TASKS · SERIAL MONITOR");Rwidget_add_child(header,title);
        serialTaskStatus_=Rlabel_create("Idle");Rwidget_add_child(header,serialTaskStatus_);
        addButton(header,"Configure...","serial.show_dialog");
        addButton(header,"Focus keyboard","serial.focus_keyboard");
        addButton(header,"Stop","serial.stop");
        addButton(header,"Clear","serial.clear");
        addButton(header,"Export TXT...","serial.export");
        auto filterRow=Rwidget_create(R_WIDGET_HBOX);Rwidget_add_child(serialTaskPage_,filterRow);
        Rwidget_add_child(filterRow,Rlabel_create("Filter:"));
        serialTaskFilterMode_=Rwidget_create(R_WIDGET_COMBO_BOX);
        for(const char* mode:{"All","Fixed text","Regex"})Rcombo_box_add_item(serialTaskFilterMode_,mode);
        const auto saved=config(pluginManager_,"filterMode","all");
        Rcombo_box_set_selected(serialTaskFilterMode_,saved=="regex"?2:saved=="fixed"?1:0);
        Rwidget_add_child(filterRow,serialTaskFilterMode_);
        serialTaskFilterInput_=Rwidget_create(R_WIDGET_TEXT_INPUT);
        Rtext_input_set_text(serialTaskFilterInput_,config(pluginManager_,"filterText","").c_str());
        Rwidget_add_child(filterRow,serialTaskFilterInput_);
        Rslot_set_fill(serialTaskFilterInput_,1.0f);
        addButton(filterRow,"Apply","serial.task_filter");
        addButton(filterRow,"Matches / All","serial.toggle_matches");
        auto txRow=Rwidget_create(R_WIDGET_HBOX);Rwidget_add_child(serialTaskPage_,txRow);
        Rwidget_add_child(txRow,Rlabel_create("TX:"));
        serialTxInput_=Rwidget_create(R_WIDGET_TEXT_INPUT);
        Rtext_input_set_placeholder(serialTxInput_,"Send command to serial device (Enter to send)...");
        Rtext_input_set_committed_callback(serialTxInput_,&EditorApp::serialTxCommitCb,this);
        Rwidget_add_child(txRow,serialTxInput_);Rslot_set_fill(serialTxInput_,1.0f);
        addButton(txRow,"Send","serial.send_line");
        addButton(txRow,"Ctrl+C","serial.send_interrupt");
        addButton(txRow,"Enter","serial.send_enter");
        serialRawCanvas_=Rwidget_create(R_WIDGET_CUSTOM);
        Rwidget_set_focusable(serialRawCanvas_,R_TRUE);
        Rwidget_set_class(serialRawCanvas_,"music-console");
        Rcustom_set_measure_callback(serialRawCanvas_,&EditorApp::serialConsoleMeasureCb,this);
        Rcustom_set_paint_callback(serialRawCanvas_,&EditorApp::serialConsolePaintCb,this);
        Rwidget_set_event_callback(serialRawCanvas_,&EditorApp::serialConsoleEventCb,this);
        Rwidget_add_child(serialTaskPage_,serialRawCanvas_);
        serialTaskLog_=Rtext_editor_create("");
        Rwidget_set_class(serialTaskLog_,"music-editor");
        Rtext_editor_set_read_only(serialTaskLog_,R_TRUE);
        Rwidget_add_child(serialTaskPage_,serialTaskLog_);
        Rslot_set_fill(serialTaskLog_,1.0f);
        Rtab_view_add_tab(terminal_tabs_,"Tasks · Serial",serialTaskPage_);
    }
    Rdock_area_open_panel(dock_,output_pane_);
    Rtab_view_show_tab(terminal_tabs_,serialTaskPage_);
    serialLastText_.clear();refreshSerialTasks();
}
void EditorApp::refreshSerialTasks(){
    if(serialTaskPage_==R_INVALID_ID||!Rwidget_is_valid(serialTaskPage_))return;
    // Keep 75 ms terminal timer responsive even with long serial captures.
    static auto last=std::chrono::steady_clock::time_point{};
    const auto now=std::chrono::steady_clock::now();
    if(now-last<std::chrono::milliseconds(220))return;
    last=now;
    const auto capture=serialCapture_.snapshot(serialOnlyMatches_,2500);
    if(serialTaskLog_!=R_INVALID_ID&&Rwidget_is_valid(serialTaskLog_)&&capture.output!=serialLastText_){
        Rtext_editor_set_text(serialTaskLog_,capture.output.c_str());
        Rtext_editor_scroll_to_line(serialTaskLog_,std::max(0,static_cast<int>(std::count(capture.output.begin(),capture.output.end(),'\n'))-1));
        serialLastText_=capture.output;
    }
    const auto state=capture.status+" | lines: "+std::to_string(capture.received)+
        (capture.dropped?" | dropped oldest: "+std::to_string(capture.dropped):"");
    if(serialTaskStatus_!=R_INVALID_ID&&Rwidget_is_valid(serialTaskStatus_))Rlabel_set_text(serialTaskStatus_,state.c_str());
    if(serialWindow_!=R_INVALID_ID&&Rwindow_is_valid(serialWindow_))label(serialWindow_,"serial_status",state);
    if(state!=serialLastStatus_){std::clog<<"[DoDevEditor][Serial] "<<state<<'\n';serialLastStatus_=state;}
}
void EditorApp::serialAction(const std::string& action){
    if(action=="serial.show_dialog"){openSerialMonitor();return;}
    if(action=="serial.focus_keyboard"){
        openSerialTasks();if(Rwidget_is_valid(serialRawCanvas_))Rwidget_focus(serialRawCanvas_);return;
    }
    if(action=="serial.send_line"){
        const char* text=serialTxInput_!=R_INVALID_ID?Rtext_input_get_text(serialTxInput_):"";
        if(text&&*text){if(serialSend(std::string(text)+"\r"))Rtext_input_set_text(serialTxInput_,"");}
        else serialSend("\r");return;
    }
    if(action=="serial.send_enter"){serialSend("\r");return;}
    if(action=="serial.send_interrupt"){serialSend(std::string(1,'\x03'));return;}

    if(action=="serial.task_filter"){
        const auto mode=serialTaskFilterMode_!=R_INVALID_ID?Rcombo_box_get_selected(serialTaskFilterMode_):0;
        const char* text=serialTaskFilterInput_!=R_INVALID_ID?Rtext_input_get_text(serialTaskFilterInput_):"";
        const std::string pattern=text?text:"";
        const auto selectedMode=mode==2?serial::FilterMode::Regex:mode==1?serial::FilterMode::Fixed:serial::FilterMode::All;
        std::string error;
        if(!serialCapture_.setFilter(selectedMode,pattern,
                serialWindow_!=R_INVALID_ID&&Rwindow_is_valid(serialWindow_)?isChecked(serialWindow_,"serial_case_sensitive"):
                truthy(config(pluginManager_,"caseSensitive","false")),&error)){
            setStatus(error);return;
        }
        pluginManager_.updatePluginSetting("com.dodev.serialmonitor","filterMode",mode==2?"regex":mode==1?"fixed":"all");
        pluginManager_.updatePluginSetting("com.dodev.serialmonitor","filterText",pattern);
        if(serialWindow_!=R_INVALID_ID&&Rwindow_is_valid(serialWindow_)){
            selectString(serialWindow_,"serial_filter_mode",mode==2?"Regex":mode==1?"Fixed text":"All");
            setInput(serialWindow_,"serial_filter_text",pattern);
        }
        serialLastText_.clear();setStatus("Serial Monitor filter applied");refreshSerialTasks();return;
    }
    if(action=="serial.toggle_matches"){
        serialOnlyMatches_=!serialOnlyMatches_;
        pluginManager_.updatePluginSetting("com.dodev.serialmonitor","showOnlyMatches",serialOnlyMatches_?"true":"false");
        if(serialWindow_!=R_INVALID_ID&&Rwindow_is_valid(serialWindow_))setChecked(serialWindow_,"serial_only_matches",serialOnlyMatches_);
        serialLastText_.clear();setStatus(serialOnlyMatches_?"Showing matching lines":"Showing all captured lines");
        refreshSerialTasks();return;
    }
    if(action=="serial.detect"){refreshSerialPortChoices();return;}
    if(action=="serial.close_dialog"){
        if(serialWindow_!=R_INVALID_ID&&Rwindow_is_valid(serialWindow_))Rwindow_destroy(serialWindow_);
        serialWindow_=R_INVALID_ID;return;
    }
    if(action=="serial.show_tasks"){openSerialTasks();return;}
    if(action=="serial.stop"){
        serialCapture_.stop();setStatus("Serial Monitor stopped");
        serialLastText_.clear();refreshSerialTasks();return;
    }
    if(action=="serial.clear"){
        serialCapture_.clear();serialLastText_.clear();
        if(serialTaskLog_!=R_INVALID_ID&&Rwidget_is_valid(serialTaskLog_))Rtext_editor_set_text(serialTaskLog_,"");
        setStatus("Serial Monitor capture cleared");return;
    }
    if(action=="serial.apply_filter"){serialApplyFilter();return;}
    if(action=="serial.export"){
        if(serialWindow_!=R_INVALID_ID&&Rwindow_is_valid(serialWindow_))serialApplyFilter();
        RFileDialogDesc desc{};
        desc.parent=serialWindow_!=R_INVALID_ID&&Rwindow_is_valid(serialWindow_)?serialWindow_:window_;
        desc.title="Export serial log to TXT";
        const std::string filename="serial_capture.txt";desc.file_name=filename.c_str();
        const char* choice=Rfile_dialog_save(&desc);
        if(!choice||!*choice)return;
        fs::path destination=fs::u8path(choice);
        if(destination.extension()!=".txt")destination+=".txt";
        const auto text=serialCapture_.exportText(serialOnlyMatches_);
        std::ofstream output(destination,std::ios::binary|std::ios::trunc);
        if(!output){setStatus("Cannot save serial log to: "+destination.string());return;}
        output.write(text.data(),static_cast<std::streamsize>(text.size()));output.flush();
        if(!output){setStatus("Serial log export failed: "+destination.string());return;}
        setStatus("Serial log exported: "+destination.string());
        std::clog<<"[DoDevEditor][Serial] Exported TXT: "<<destination<<" ("<<text.size()<<" bytes)\n";
        return;
    }
    if(action=="serial.start"){
        if(serialWindow_==R_INVALID_ID||!Rwindow_is_valid(serialWindow_))return;
        if(!serialApplyFilter())return;
        serial::PortOptions options;
        options.port=input(serialWindow_,"serial_port");
        try{
            options.baud=std::stoi(selected(serialWindow_,"serial_baud"));
            options.dataBits=std::stoi(selected(serialWindow_,"serial_data_bits"));
            options.stopBits=std::stoi(selected(serialWindow_,"serial_stop_bits"));
        }catch(...){setStatus("Invalid serial port settings");return;}
        const auto parity=selected(serialWindow_,"serial_parity");options.parity=parity.empty()?'N':parity[0];
        options.allowPolkit=isChecked(serialWindow_,"serial_polkit");
#ifndef _WIN32
        // Use an absolute, application-controlled helper path. Never invoke a shell
        // or accept the elevated executable path from user settings.
        std::error_code ec;
#if defined(__linux__)
        const auto executable=fs::read_symlink("/proc/self/exe",ec);
        if(!ec)options.privilegedHelper=(executable.parent_path()/"dodev-serial-reader-helper").string();
#endif
#endif
        std::string error;if(!serialCapture_.start(options,&error)){
            label(serialWindow_,"serial_status",error);setStatus(error);return;
        }
        pluginManager_.updatePluginSetting("com.dodev.serialmonitor","port",options.port);
        pluginManager_.updatePluginSetting("com.dodev.serialmonitor","baud",std::to_string(options.baud));
        pluginManager_.updatePluginSetting("com.dodev.serialmonitor","dataBits",std::to_string(options.dataBits));
        pluginManager_.updatePluginSetting("com.dodev.serialmonitor","stopBits",std::to_string(options.stopBits));
        pluginManager_.updatePluginSetting("com.dodev.serialmonitor","parity",std::string(1,options.parity));
        pluginManager_.updatePluginSetting("com.dodev.serialmonitor","allowPolkit",options.allowPolkit?"true":"false");
        openSerialTasks();
        std::clog<<"[DoDevEditor][Serial] Starting capture: "<<options.port<<" @ "<<options.baud<<"\n";
        setStatus("Serial Monitor started: "+options.port);return;
    }
}
bool EditorApp::serialSend(const std::string& bytes){
    std::string error;
    if(!serialCapture_.sendBytes(bytes,&error)){setStatus("Serial TX: "+error);return false;}
    return true;
}
void EditorApp::serialTxCommitCb(Rid,const char*,void* ctx){
    static_cast<EditorApp*>(ctx)->serialAction("serial.send_line");
}
void EditorApp::serialConsoleMeasureCb(Rid,float* width,float* height,void*){
    if(width)*width=260.0f;if(height)*height=36.0f;
}
void EditorApp::serialConsolePaintCb(Rid,RPainter* p,float width,float height,void* ctx){
    auto* app=static_cast<EditorApp*>(ctx);
    const bool connected=app->serialCapture_.running();
    Rdraw_rect(p,0,0,width,height,8,0.08f,0.15f,0.20f,1.0f);
    Rdraw_rect_outline(p,0,0,width,height,8,1.0f,0.25f,0.73f,0.62f,0.65f);
    Rdraw_text(p,Rfont_get_monospace(),13,12,8,
        connected?"SERIAL KEYBOARD - click here, then type (arrows / Ctrl+C / Enter)":"SERIAL KEYBOARD - start capture first",
        0.75f,0.95f,0.88f,1.0f);
}
int EditorApp::serialConsoleEventCb(Rid widget,const REvent* e,void* ctx){
    if(!e)return R_FALSE;
    auto& app=*static_cast<EditorApp*>(ctx);
    if(e->type==R_EVENT_MOUSE_DOWN){Rwidget_focus(widget);return R_TRUE;}
    if(e->type==R_EVENT_TEXT){
        if(e->modifiers&R_MOD_CTRL)return R_TRUE;
        const auto cp=e->codepoint;
        if(cp<0x80){std::string b(1,static_cast<char>(cp));
            if(e->modifiers&R_MOD_ALT)b.insert(0,1,'\x1b');
            app.serialSend(b);
        }else if(cp<=0x10FFFF){
            // UTF-8; only serial devices configured for Unicode will interpret it.
            std::string bytes;
            if(cp<0x800){bytes+=static_cast<char>(0xC0|(cp>>6));bytes+=static_cast<char>(0x80|(cp&63));}
            else if(cp<0x10000){bytes+=static_cast<char>(0xE0|(cp>>12));bytes+=static_cast<char>(0x80|((cp>>6)&63));bytes+=static_cast<char>(0x80|(cp&63));}
            else {bytes+=static_cast<char>(0xF0|(cp>>18));bytes+=static_cast<char>(0x80|((cp>>12)&63));bytes+=static_cast<char>(0x80|((cp>>6)&63));bytes+=static_cast<char>(0x80|(cp&63));}
            if(e->modifiers&R_MOD_ALT)bytes.insert(0,1,'\x1b');
            app.serialSend(bytes);
        }
        return R_TRUE;
    }
    if(e->type!=R_EVENT_KEY_DOWN)return R_FALSE;
    const bool ctrl=(e->modifiers&R_MOD_CTRL)!=0;
    if(ctrl&&e->key>=R_KEY_A&&e->key<=R_KEY_Z){
        app.serialSend(std::string(1,static_cast<char>(e->key-R_KEY_A+1)));return R_TRUE;
    }
    const char* seq=nullptr;
    switch(e->key){
        case R_KEY_ENTER:case R_KEY_KP_ENTER:seq="\r";break;
        case R_KEY_BACKSPACE:seq="\x7f";break;
        case R_KEY_TAB:seq="\t";break;
        case R_KEY_ESCAPE:seq="\x1b";break;
        case R_KEY_UP:seq="\x1b[A";break;
        case R_KEY_DOWN:seq="\x1b[B";break;
        case R_KEY_LEFT:seq="\x1b[D";break;
        case R_KEY_RIGHT:seq="\x1b[C";break;
        case R_KEY_HOME:seq="\x1b[H";break;
        case R_KEY_END:seq="\x1b[F";break;
        case R_KEY_DELETE:seq="\x1b[3~";break;
        case R_KEY_PAGE_UP:seq="\x1b[5~";break;
        case R_KEY_PAGE_DOWN:seq="\x1b[6~";break;
        default:break;
    }
    if(seq){std::string bytes=seq;
        if(e->modifiers&R_MOD_ALT)bytes.insert(0,1,'\x1b');
        app.serialSend(bytes);return R_TRUE;}
    return R_FALSE;
}
} // namespace dodev
