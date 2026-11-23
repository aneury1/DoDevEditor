#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace dodev::terminal {
namespace fs=std::filesystem;
struct Task {
    std::string label;
    std::string command;
    std::string group;
    std::vector<std::string> arguments;
    std::string cwd;
};
std::vector<Task> loadTasks(const fs::path& path,std::string* error=nullptr);
std::string quoteArgument(const std::string& value,bool windows=false);
std::string commandLine(const Task& task,bool windows=false);
std::string launchFileCommand(const fs::path& file,bool windows=false);
std::string defaultTasksJson();
}
