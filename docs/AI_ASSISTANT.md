# AI Assistant plugin

See [Release 3.7, section 3](RELEASE_3_7.md#3-ai-assistant-plugin).

The bundled native plugin resides in `thirdparty/AIAssistant/` and uses the **additive SDK v1** `open_ai_assistant` host callback. The host creates and owns the Raven dock panel, while `AiClient` handles Chat Completions JSON and optional libcurl transport.

**Local setup:** start Ollama and select its OpenAI-compatible endpoint. **Remote setup:** configure an HTTPS provider and make the API token available as an environment variable to the DoDevEditor process. Configuring an environment-variable **name** does not store the secret itself.

Nothing is sent when simply opening an editor tab. The `Ask` command sends only your typed prompt; `Explain File`, `Explain Selection`, and `Review File` explicitly include active-editor source text. Generated text is never written back to the active document automatically.
