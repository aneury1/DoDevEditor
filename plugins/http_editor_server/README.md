# HTTP + WebSocket Editor Server Plugin

`org.dodev.http-editor-server` exposes open DoDevEditor buffers through a small, dependency-free network service.

Features:

- versioned REST API under `/api/v1/...`;
- compatibility SSE stream for the active editor;
- RFC 6455 WebSocket server implemented with native sockets only;
- one room/channel per open editor file;
- multiplexed room subscriptions or direct room WebSocket URLs;
- variable/concurrent client count (one worker per accepted client; no fixed client array);
- token authentication;
- current unsaved editor text, caret, selection, path, title, and generation;
- plugin logging for requests, WebSocket messages, file events, errors, and lifecycle actions;
- no plugin-side message boxes;
- no wxWidgets link dependency in the plugin.

See [API.md](API.md) for endpoint nomenclature, room semantics, WebSocket messages, examples, authentication, concurrency, and logging behavior.

## Default configuration

On first load the plugin creates `dodev_http_editor_server.json` beside its `.so`/`.dll`:

```json
{
  "bind_address": "0.0.0.0",
  "port": 9934,
  "auth_token": "",
  "max_text_bytes": 4194304
}
```

Environment variables override the JSON file:

- `DODEV_HTTP_BIND`
- `DODEV_HTTP_PORT`
- `DODEV_HTTP_TOKEN`
- `DODEV_HTTP_MAX_TEXT_BYTES`

DoDevEditor **General Settings -> Plugins -> HTTP / WebSocket Editor Server** writes the same runtime values through these overrides. Saving settings reloads the plugin set so network changes take effect.

`0.0.0.0` listens on every network interface. Configure a token when the server is reachable by other machines.

## Primary endpoints

```text
GET /api/v1/health
GET /api/v1/editors/active
GET /api/v1/files/rooms
GET /api/v1/files/rooms/{room}
GET /api/v1/editors/active/events
GET /ws/v1/files                  WebSocket upgrade
GET /ws/v1/files/{room}           WebSocket direct room
```

Every endpoint is emitted as a log line when the server starts.

## Build standalone

Linux:

```bash
cmake -S plugins/http_editor_server -B build/http-editor-server \
  -DDODEV_SDK_ROOT="$PWD"
cmake --build build/http-editor-server
```

Windows can build the same CMake project with MSVC or MinGW. Only system socket/thread libraries are used.
