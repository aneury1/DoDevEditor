#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "dodev/sdk/dodev_plugin_api.h"

namespace {
void log(const DodevHostAPI* host, int severity, const std::string& text) {
  if (host->log) host->log(host->context, severity, text.c_str());
}
std::string get(const DodevHostAPI* host, const char* key,
                const char* fallback) {
  const char* result = host->get_setting(host->context, key);
  return result && *result ? result : fallback;
}
std::string quote(std::string value) {
#ifdef _WIN32
  // Double quotes work for simple literal arguments in both cmd.exe and
  // PowerShell. Reject metacharacters rather than relying on shell-specific
  // escaping rules.
  for (char c : value)
    if (c == '"' || c == '%' || c == '!' || c == '^' || c == '&' || c == '|' ||
        c == '<' || c == '>')
      return {};
  return "\"" + value + "\"";
#else
  std::string result = "'";
  for (char c : value) {
    if (c == '\'')
      result += "'\\''";
    else
      result += c;
  }
  return result + "'";
#endif
}
// Parse user input into literal argv-like tokens; never evaluate shell
// operators.
bool splitArgs(const std::string& input, std::vector<std::string>& args) {
  args.clear();
  std::string token;
  char quoteChar = 0;
  bool escaped = false, started = false;
  for (char c : input) {
    if (c == '\0' || c == '\r' || c == '\n') return false;
    if (escaped) {
      token += c;
      escaped = false;
      started = true;
      continue;
    }
#ifdef _WIN32
    if (c == '\\') {
      token += c;
      started = true;
      continue;
    }
#else
    if (c == '\\' && quoteChar != '\'') {
      escaped = true;
      started = true;
      continue;
    }
#endif
    if (quoteChar) {
      if (c == quoteChar)
        quoteChar = 0;
      else
        token += c;
      started = true;
      continue;
    }
    if (c == '\'' || c == '"') {
      quoteChar = c;
      started = true;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (started) {
        args.push_back(token);
        token.clear();
        started = false;
      }
    } else {
      token += c;
      started = true;
    }
  }
  if (escaped || quoteChar) return false;
  if (started) args.push_back(token);
  if (args.size() > 128) return false;
  for (const auto& arg : args)
    if (arg.size() > 4096) return false;
  return true;
}
void execute(const DodevHostAPI* host, const std::vector<std::string>& args) {
  if (!DODEV_HOST_HAS(host, execute_terminal_command)) {
    log(host, 2, "Terminal command bridge is not available in this host");
    return;
  }
  const std::string executable = get(host, "executable", "docker");
  if (executable.empty() ||
      executable.find_first_of("\r\n") != std::string::npos) {
    log(host, 2, "Invalid docker executable setting");
    return;
  }
  std::string command = quote(executable);
  if (command.empty()) {
    log(host, 2, "Unsupported character in Docker executable path");
    return;
  }
  std::vector<std::string> effective;
  auto context = get(host, "context", ""), daemon = get(host, "host", ""),
       config = get(host, "configDirectory", "");
  if (!context.empty() && !daemon.empty()) {
    log(host, 2, "Choose a Docker context OR a host");
    return;
  }
  if (!context.empty()) {
    effective.push_back("--context");
    effective.push_back(context);
  }
  if (!daemon.empty()) {
    effective.push_back("--host");
    effective.push_back(daemon);
  }
  if (!config.empty()) {
    effective.push_back("--config");
    effective.push_back(config);
  }
  auto flags = get(host, "globalFlags", "");
  std::string line;
  for (char ch : flags) {
    if (ch == '\n') {
      if (!line.empty()) effective.push_back(line);
      line.clear();
    } else if (ch != '\r')
      line += ch;
  }
  if (!line.empty()) effective.push_back(line);
  effective.insert(effective.end(), args.begin(), args.end());
  for (const auto& a : effective) {
    const auto literal = quote(a);
    if (literal.empty()) {
      log(host, 2, "Unsupported shell metacharacter in Docker CLI argument");
      return;
    }
    command += ' ' + literal;
  }
  if (!host->execute_terminal_command(host->context, command.c_str()))
    log(host, 2, "Cannot run Docker command in terminal");
}
void ps(const DodevHostAPI* h, void*) { execute(h, {"ps", "-a"}); }
void images(const DodevHostAPI* h, void*) { execute(h, {"images"}); }
void version(const DodevHostAPI* h, void*) { execute(h, {"version"}); }
void composeUp(const DodevHostAPI* h, void*) {
  execute(h,
          {"compose", "-f", get(h, "composeFile", "compose.yaml"), "up", "-d"});
}
void composeDown(const DodevHostAPI* h, void*) {
  execute(h, {"compose", "-f", get(h, "composeFile", "compose.yaml"), "down"});
}
void build(const DodevHostAPI* h, void*) {
  execute(h, {"build", "-t", get(h, "buildTag", "dodev-app:latest"),
              get(h, "buildContext", ".")});
}
void logPrompt(const DodevHostAPI* host, const char* answer, void*) {
  if (answer && *answer) execute(host, {"logs", "--tail", "200", answer});
}
void logs(const DodevHostAPI* host, void*) {
  if (!DODEV_HOST_HAS(host, prompt_input)) {
    log(host, 2, "Plugin input dialog unavailable");
    return;
  }
  const auto container = get(host, "container", "");
  host->prompt_input(host->context, "Docker Logs",
                     "Container name or ID:", container.c_str(), &logPrompt,
                     nullptr);
}
void runPrompt(const DodevHostAPI* host, const char* answer, void*) {
  if (answer && *answer) execute(host, {"run", "--rm", "-it", answer});
}
void run(const DodevHostAPI* host, void*) {
  if (!DODEV_HOST_HAS(host, prompt_input)) return;
  const auto image = get(host, "image", "alpine:latest");
  host->prompt_input(host->context, "Docker Run",
                     "Docker image:", image.c_str(), &runPrompt, nullptr);
}
void customDone(const DodevHostAPI* host, const char* text, void*) {
  if (!text || !*text) return;
  std::vector<std::string> args;
  if (!splitArgs(text, args)) {
    log(host, 2, "Unbalanced quotation or unsupported input");
    return;
  }
  if (!args.empty() && args.front() == "docker") args.erase(args.begin());
  if (args.empty()) {
    log(host, 2, "Enter Docker arguments, e.g. ps -a");
    return;
  }
  execute(host, args);
}
void customPrompt(const DodevHostAPI* host, void*) {
  if (!DODEV_HOST_HAS(host, prompt_input)) return;
  host->prompt_input(
      host->context, "Docker CLI Prompt",
      "Docker CLI arguments (e.g. ps -a, compose ps, inspect container):",
      "ps -a", &customDone, nullptr);
}
void dockerTab(const DodevHostAPI*, const DodevUiBuilder* ui, void*) {
  ui->label(ui->context, "Docker · Integrated Terminal Commands");
  ui->button(ui->context, "List containers", "docker.ps");
  ui->button(ui->context, "List images", "docker.images");
  ui->button(ui->context, "Compose up -d", "docker.compose_up");
  ui->button(ui->context, "Compose down", "docker.compose_down");
  ui->button(ui->context, "Build image", "docker.build");
  ui->button(ui->context, "Container logs...", "docker.logs");
  ui->button(ui->context, "Run image...", "docker.run");
  ui->button(ui->context, "Docker CLI prompt...", "docker.prompt");
  ui->read_only_text(
      ui->context,
      "Requires Docker CLI and daemon installed locally.\nAll commands are "
      "sent to the selected PTY/ConPTY terminal.\nCompose down stops services; "
      "review your Docker environment before use.");
}
}  // namespace
extern "C" DODEV_PLUGIN_EXPORT int dodev_plugin_init(const DodevHostAPI* host,
                                                     DodevPluginAPI* plugin) {
  if (!host || !plugin || host->api_version != DODEV_PLUGIN_API_VERSION ||
      host->struct_size < offsetof(DodevHostAPI, subscribe_event) +
                              sizeof(host->subscribe_event) ||
      plugin->struct_size < sizeof(DodevPluginAPI))
    return 0;
  plugin->api_version = DODEV_PLUGIN_API_VERSION;
  plugin->shutdown = nullptr;
  const struct {
    const char* name;
    const char* def;
    const char* description;
  } settings[] = {
      {"executable", "docker", "Docker CLI executable or absolute path"},
      {"context", "", "Docker context for the graphical manager"},
      {"host", "", "Docker daemon URL for the graphical manager"},
      {"configDirectory", "", "Docker CLI config directory"},
      {"globalFlags", "",
       "Additional global Docker CLI arguments, one per line"},
      {"composeFile", "compose.yaml", "Docker Compose YAML filename"},
      {"buildTag", "dodev-app:latest", "Default docker build -t tag"},
      {"buildContext", ".",
       "docker build context relative to terminal working directory"},
      {"container", "", "Default Docker container for logs"},
      {"image", "alpine:latest", "Default image for docker run"}};
  for (const auto& item : settings)
    if (!host->register_setting(host->context, item.name, item.def,
                                item.description))
      return 0;
  const struct {
    const char* id;
    const char* label;
    DodevCommandFn command;
  } actions[] = {
      {"docker.ps", "Docker: List Containers", &ps},
      {"docker.images", "Docker: List Images", &images},
      {"docker.version", "Docker: Version", &version},
      {"docker.compose_up", "Docker: Compose Up", &composeUp},
      {"docker.compose_down", "Docker: Compose Down", &composeDown},
      {"docker.build", "Docker: Build Image", &build},
      {"docker.logs", "Docker: Container Logs...", &logs},
      {"docker.run", "Docker: Run Image...", &run},
      {"docker.prompt", "Docker: Execute Command Prompt...", &customPrompt}};
  for (const auto& action : actions)
    if (!host->register_command(host->context, action.id, action.label,
                                action.command, nullptr))
      return 0;
  for (const auto& action : actions)
    host->register_menu(host->context, "Tools", action.label, action.id);
  host->register_tab(host->context, "docker.dashboard", "Docker Manager",
                     &dockerTab, nullptr);
  return 1;
}
