# DoDevEditor HTTP Client 1.0

Requires DoDevEditor 5.2.8+ (generic form and cancellable job extensions). The plugin is a native shared library using only the public C SDK, libcurl, and SQLite. No wxWidgets or editor libraries are linked.

## Use

Enable HTTP Client in Settings > Plugins and restart. Open a workspace. The Workspace notebook has **HTTP Client** and **HTTP History** tabs. Enter an uppercase method (GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS or other HTTP token), a full URL including query string, one `Name: Value` header per line, and an optional text body. Use Content-Type for JSON/XML and Authorization for credentials. Press Send request. A cancellable progress dialog appears while libcurl executes the request on a worker; after completion, cancellation or failure, a unique read-only editor tab displays request and response, status, duration, body size, response headers, and body. Existing tabs stay open. History selection opens a new editor tab without transmitting anything. Refresh history with its button or Request history from the builder.

Example: POST http://localhost:8080/api/items with `Content-Type: application/json` and `{"name":"Test"}`.

## Settings and persistence

Settings > HTTP Client: default URL, total timeout (1–300 seconds), history limit (1–1000), history mode (metadata/full/off). Global settings belong to the plugin. Each workspace stores history in `.dodev/plugins/com.dodev.httpclient/history.sqlite`. Metadata (default) retains method, URL origin only, UTC date and status. Full mode stores complete request/response report as unencrypted text; sensitive headers, query strings and payloads may be included. Do not enable for payment/card data or sensitive credentials. Add `.dodev/plugins/com.dodev.httpclient/` to your project's Git ignore list. Changing mode does not delete old entries. Clear history requires typing CLEAR; backups and storage-level copies may persist. Current editor tabs show submitted credentials in plaintext; close them when no longer needed.

TLS host and peer verification are enabled. Redirects are returned rather than followed, to avoid forwarding credentials. The system/libcurl proxy and CA configuration apply. The body cap is 2 MiB per request or response; headers cap is 128 KiB. Invalid UTF-8 bytes are escaped for display. This is a text API client, not a binary downloader. No request collection/replay, multipart file upload, automatic redirect, parallel send, or separate auth wizard yet. History refresh is explicit. A modal progress dialog blocks additional sends until the current request completes. Cancellation depends on libcurl progress callbacks and resolver behavior.

## Build independently

The standalone archive includes the public SDK headers in `sdk/include`. In the source repository, use `-DDODEV_SDK_INCLUDE=/path/to/DoDevEditor/include` when building this directory alone.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
sudo cmake --install build --prefix /usr/local
```

Dependencies: Arch/CachyOS `curl sqlite pkgconf`; Debian/Ubuntu `libcurl4-openssl-dev libsqlite3-dev pkg-config`. Python 3 is only needed for local test fixture. For the editor build, `DODEV_BUILD_HTTP_CLIENT_PLUGIN=OFF` excludes the plugin. Missing dependencies skip it in the parent build. Native GUI and non-Linux platforms require local verification.

## SDK design

`DodevUiBuilder::form` copies field definitions synchronously and submits a borrowed string array on the GUI thread. `DodevHostAPI::run_job` runs the worker with a cancellation callback; it waits for worker completion and the worker cannot call host UI APIs. The plugin then calls `show_text_view` with a unique ID and `Editor` placement. Both API additions are appended ABI v1 fields, gated by `struct_size`. Existing plugins remain compatible. The plugin registers all settings, menus and views itself.
