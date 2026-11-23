# DoDevEditor Raven 3.8 — AI Assistant configuration

## Feature overview

The bundled **AI Assistant** plugin now supports separate, named provider profiles. It has three provider types:

| Type | Default Chat endpoint | Models endpoint | Authentication |
|---|---|---|---|
| Local llama.cpp | `http://127.0.0.1:8080/v1/chat/completions` | `/v1/models` | None |
| Local Ollama | `http://127.0.0.1:11434/v1/chat/completions` | `/v1/models` | None |
| Custom remote (OpenAI-compatible) | Enter a provider-specific HTTPS Chat Completions URL | Automatically derived or user-supplied | None or Bearer API key from environment |

The configured llama.cpp port is **editable**: use the port actually used by your `llama-server` instance (for example 8080 or 9931).

### Launching llama.cpp

```bash
llama-server -m /path/to/your-model.gguf --host 127.0.0.1 --port 8080
curl http://127.0.0.1:8080/v1/models
```

DoDevEditor sends `GET /v1/models` and reads each `data[].id` in the standard OpenAI-compatible response. This discovery operation runs in a worker thread; it will not block the GUI. The selected model identifier is placed into the editable **Model ID** field. **Refresh Models** allows another attempt after loading a different model. When a local profile is first opened or selected, an automatic refresh is attempted.

## Using the settings panel

1. Enable `com.dodev.aiassistant` in **Settings → Plugins** and restart.
2. Open **Herramientas → AI Assistant...** or **Settings → Plugins → AI Provider Profiles...**.
3. Choose a saved profile. To add a server, click **+ Remote** or **+ Local**.
4. Enter a **Profile name**, provider type, full Chat endpoint, optional Models endpoint, authentication mode, environment variable name, and request timeout.
5. Press **Refresh Models** to populate the model dropdown, or enter a model ID manually if discovery is unsupported.
6. Click **Save Profile** to persist the profiles and the current active profile. Changing the profile later restores its individual settings.
7. Ask a question, or explicitly use **Explain File**, **Explain Selection**, or **Review File** to send source context.

**Note:** local llama.cpp normally reports the currently loaded model, not a list of models installed somewhere on disk. The dropdown reflects the models reported by that server instance.

## Remote provider configuration

Use **+ Remote** to add a named provider, for example:

- Profile name: `My Remote Coding API`
- Chat endpoint: `https://example-provider.invalid/v1/chat/completions` *(replace with the actual provider URL)*
- Models endpoint (optional): `https://example-provider.invalid/v1/models`
- Authentication: `Bearer token from environment`
- Key variable: `MY_CODING_API_TOKEN`
- Model ID: a provider-compatible identifier, selected from the model list if supported

The **Models endpoint** input can be left blank for ordinary OpenAI-compatible endpoints ending in `/chat/completions`. In that case it is derived automatically (e.g., `/v1/models`). Some remote providers do not support a model listing; this is not a fatal error, because the Model ID field remains editable.

For an authenticated remote provider, start the editor from an environment that contains the token:

```bash
export MY_CODING_API_TOKEN='your-provider-token'
DoDevEditor
```

For privacy, the plugin **does not save the token**, only the environment variable's name. Remote non-loopback HTTP is rejected; use HTTPS, or tunnel an HTTP server securely to the local loopback interface. Redirects are disabled and TLS certificates are checked. Authenticated model discovery refuses a Models endpoint on a different origin than the Chat endpoint to avoid leaking Bearer credentials.

## Optional JSON settings

On Linux the plugin settings are stored at:

```text
${XDG_CONFIG_HOME:-$HOME/.config}/DoDevEditor/plugins/com.dodev.aiassistant/settings.json
```

An example of the structured JSON document saved by **Save Profile** is:

```json
{
  "activeProfileId": "local-llamacpp",
  "profiles": [
    {
      "id": "local-llamacpp",
      "name": "llama.cpp (local)",
      "kind": "llama.cpp",
      "endpoint": "http://127.0.0.1:8080/v1/chat/completions",
      "modelsEndpoint": "",
      "model": "my-loaded-model-id",
      "authMode": "none",
      "keyEnvironment": "",
      "timeoutSeconds": 45,
      "maxContextBytes": 16000
    },
    {
      "id": "remote-1",
      "name": "My Remote Coding API",
      "kind": "remote",
      "endpoint": "https://example-provider.invalid/v1/chat/completions",
      "modelsEndpoint": "",
      "model": "provider-model-id",
      "authMode": "bearer",
      "keyEnvironment": "MY_CODING_API_TOKEN",
      "timeoutSeconds": 60,
      "maxContextBytes": 16000
    }
  ]
}
```

A missing or invalid profiles array causes built-in local profiles to be used. Existing legacy single-endpoint settings are imported when customized. Save uses the project's atomic JSON writing mechanism. It stores **configuration, not API secrets or conversation history**.

## Build and install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel
ctest --test-dir build --parallel 4 --output-on-failure
cmake --install build
```

To run all headless tests without downloading Raven:

```bash
cmake -S . -B build-headless -DDODEV_BUILD_GUI=OFF -DDODEV_BUILD_TESTS=ON
cmake --build build-headless --parallel
ctest --test-dir build-headless --parallel 4 --output-on-failure
```

The AI module is located at `bin/plugins/ai-assistant/` after installation. `libcurl` development libraries are required for network functionality. If libcurl is unavailable, the UI still runs but HTTP model discovery and chat respond with an explanatory error.

## Troubleshooting

- **Connection refused:** start llama-server, check the port and verify `curl http://127.0.0.1:PORT/v1/models`.
- **HTTP 401 or 403:** select Bearer authentication and set the corresponding environment variable before launching DoDevEditor.
- **HTTP 404 on models:** configure a custom Models endpoint, or enter a model ID manually.
- **No model ID:** choose a detected model or type its exact ID in the model field.
- **No saved configuration:** verify write access to your DoDevEditor configuration directory.
- **No AI menu:** enable the native AI Assistant plugin and restart.
- **No remote HTTP support:** remote endpoints require HTTPS; use a secure local tunnel for private HTTP deployments.

### Compatibility and limits

This release uses **OpenAI-compatible Chat Completions** and **GET /v1/models**. Remote APIs with incompatible schemas need an adapter. Some providers expose model discovery at a different path; configure the optional Models endpoint, and use manual model selection as a fallback. The full Raven/Vulkan GUI must still be validated on a machine with Raven dependencies installed.
