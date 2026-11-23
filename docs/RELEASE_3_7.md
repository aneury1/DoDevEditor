# DoDevEditor Raven 3.7 — AI Assistant and Clang-first Source Symbols

This release builds on the **corrected** 3.6 source package (including the valid `EmbeddedJournalUi.h` raw string). It retains the interactive serial monitor, source outline, remote journal, QR code, terminal, menus and theme system.

## 1. Build and install

```bash
# On Debian/Ubuntu: sudo apt install cmake git clang libcurl4-openssl-dev
# On Arch/CachyOS: sudo pacman -S cmake git clang curl
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build
```

If Raven has already been downloaded, point `-DFETCHCONTENT_SOURCE_DIR_RAVEN=/path/to/Raven` at that checkout. Otherwise CMake fetches `https://github.com/aneury1/Raven.git` with Git progress visible. To run unit tests without Raven/Vulkan, configure `-DDODEV_BUILD_GUI=OFF`.

`make install` (for Makefile generators) / `cmake --install` places the two bundled plugins alongside the pre-existing plugins:

```text
<prefix>/bin/plugins/source-symbols/plugin.json
<prefix>/bin/plugins/source-symbols/libdodev_source_symbols_plugin.so
<prefix>/bin/plugins/ai-assistant/plugin.json
<prefix>/bin/plugins/ai-assistant/libdodev_ai_assistant_plugin.so
```

Use **Settings → Plugins** to enable `com.dodev.sourcesymbols` and `com.dodev.aiassistant`, then restart. Bundled native plugins remain **opt-in** for security.

## 2. Clang-first Symbols / Outline

Open a C/C++ file, select **Herramientas → Symbols / Outline...**, and wait for the parser to finish. The Tasks · Symbols panel shows a table with `Line`, `Kind`, `Symbol`, and `Signature`, plus the parser name and any fallback reason. The symbols refer to the **active in-memory editor buffer**, including unsaved edits.

The **Auto** parser policy is:

1. Try an explicitly configured `clangExecutable`; otherwise `DODEV_CLANG_EXECUTABLE`; otherwise search `PATH` for `clang++`, followed by `clang`.
2. Start the compiler directly (no shell) in a bounded analysis, using `-fsyntax-only -Xclang -ast-dump=json`. Source text is passed via a temporary stdin file. The file's parent directory is on the include path.
3. Use Clang's JSON AST to report C/C++ namespaces, classes, methods, functions, enums, enum constants, fields, typedefs, variables and concepts with precise offsets/line/column navigation. Macro and include lines are supplemented from the lexical scanner.
4. If Clang is unavailable, exits with parsing errors, produces no useful AST, takes too long, or emits more than 16 MiB of AST JSON, fall back to the original regex parser. The UI shows `Regex (fallback)` and a diagnostic.
5. Python, Kotlin, Java, JavaScript and TypeScript retain their existing regex-based extractors.

### Plugin settings

Open **Settings → Plugins → Source Symbols** and edit these preferences:

| Key | Allowed values / default | Meaning |
|---|---|---|
| `parserMode` | `auto` (default), `clang`, `regex` | Clang first with regex fallback, strict Clang only, or regex only |
| `clangExecutable` | Empty (default), executable name or path | Override detected compiler |
| `timeoutMs` | `2500` (default) | Maximum time for a Clang process (bounded to 250–20,000 ms) |

You may also set `DODEV_CLANG_EXECUTABLE=/usr/bin/clang++` before launching the editor. The plugin preferences are optional JSON; missing files do not block startup.

**Limitations:** the Clang subprocess integration is implemented for POSIX (Linux/macOS) in this release. Windows builds use regex fallback; support for Windows `CreateProcess` remains outstanding. This first version does not consume the project's `compile_commands.json`, so projects requiring complex compiler flags or very large C++ header ASTs may fall back to regex. Source parsing currently runs synchronously when you press Refresh; large sources may briefly occupy the UI thread, bounded by the configured timeout.

## 3. AI Assistant plugin

Open **Herramientas → AI Assistant...** after enabling `com.dodev.aiassistant`. A right-side Raven panel includes a provider endpoint field, model identifier, environment-variable **name**, message input, conversation transcript, and action buttons.

| Action | Source text sent? |
|---|---|
| **Ask** | No file, only your typed question and recent chat history |
| **Explain File** | Yes, **only** when clicked; active unsaved document is attached |
| **Explain Selection** | Yes, **only** when clicked; highlighted source is attached |
| **Review File** | Yes, **only** when clicked; active unsaved document is attached |
| **Clear** | Clears in-memory chat history (does not delete your files) |
| **Cancel** | Cancels the ongoing HTTP request |

Chat requests run on a worker thread to keep the editor responsive. The app never applies AI-generated edits automatically. Responses appear in the panel for review/copying. History is stored only in memory for the current process; the API token is **never written to settings.json**.

### Option A: Local Ollama

Install and start Ollama independently, then pull a model:

```bash
ollama pull llama3.2
ollama serve
```

Configure the AI panel:

```text
Endpoint: http://127.0.0.1:11434/v1/chat/completions
Model:    llama3.2
```

A local HTTP endpoint is accepted only on the loopback addresses `127.0.0.1`, `localhost`, or `[::1]`. Different local servers providing OpenAI-compatible `/v1/chat/completions` endpoints are also supported.

### Option B: OpenAI-compatible remote API

In the terminal from which you start DoDevEditor, export a token. The example below is deliberately a placeholder; do not put real secrets into shell history or configuration files.

```bash
export OPENAI_API_KEY='YOUR_API_KEY_FROM_YOUR_PROVIDER'
~/.local/bin/DoDevEditor
```

In the AI panel:

```text
Endpoint:         https://api.openai.com/v1/chat/completions
Model:            <your-accessible-model-name>
Key environment:  OPENAI_API_KEY
```

Remote API endpoints must use HTTPS. Redirects are disabled, TLS certificates are verified and the request is capped at 45 seconds by default. The text file is truncated to the configured context budget (default 16,000 bytes) rather than uploading arbitrary large buffers. With a commercial provider, requests may incur costs and data is transmitted to that provider only after pressing the corresponding button.

### Plugin settings (optional JSON)

```json
{
  "endpoint": "http://127.0.0.1:11434/v1/chat/completions",
  "model": "llama3.2",
  "keyEnvironment": "OPENAI_API_KEY",
  "timeoutSeconds": "45",
  "maxContextBytes": "16000"
}
```

Use the editor's **Settings → Plugins** or the panel's **Save Settings** action. Do not put the key itself in this file.

**Requires:** libcurl development headers at build time. If libcurl is unavailable, the project can still build but AI requests return a clear unavailable-transport error. Network responses are capped at 2 MiB. Remote AI connections have not been tested in this container; request encoding and JSON parsing were tested separately.

## 4. Verification

```bash
cmake -S . -B build-core -DDODEV_BUILD_GUI=OFF -DDODEV_BUILD_TESTS=ON
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

The regression suite includes Clang AST extraction, forced-regex mode, Clang-not-installed fallback, AI request/response JSON, AI native plugin activation, and all prior serial, journal, QR, settings, keybindings and terminal tests.

The Raven/Vulkan graphical build and visual interaction remain unverified in this build environment because the Raven dependency source is not available for compilation here.
