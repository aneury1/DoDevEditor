#include "dodev/terminal/TerminalTasks.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(){using namespace dodev::terminal;
 auto p=std::filesystem::temp_directory_path()/"dodev_tasks_integration.json";
 {std::ofstream o(p);o<<defaultTasksJson();}
 std::string err;auto tasks=loadTasks(p,&err);assert(err.empty());assert(tasks.size()==3);assert(tasks[1].group=="build");
 assert(commandLine(tasks[1]).find("cmake --build")==0);
 assert(quoteArgument("my 'file'.cpp")=="'my '\\''file'\\''.cpp'");
 assert(launchFileCommand("test.py")=="python3 'test.py'");
 assert(launchFileCommand("test.cpp").empty());
 std::filesystem::remove(p);
 assert(loadTasks(p).empty());
 std::cout<<"terminal tasks passed\n";
}
