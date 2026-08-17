# HTTP + WebSocket Editor Server API

Protocol version: **v1**  
Plugin: `org.dodev.http-editor-server`  
Default listener: `0.0.0.0:9934`

The plugin uses only operating-system sockets, C++17, and the DoDevEditor plugin ABI. It does **not** depend on a third-party HTTP or WebSocket library.

## Endpoint naming convention

New endpoints use a stable versioned namespace:

- REST resources: `/api/v1/<resource>`
- Server-Sent Events: `/api/v1/<resource>/events`
- WebSocket transports: `/ws/v1/<resource>`
- File-room WebSockets: `/ws/v1/files/{room}`

Endpoint names used in DoDevEditor logs follow `transport.resource-name`, for example `api.file-rooms` and `ws.file-room`.

| Log name | Method | Endpoint | Purpose |
|---|---|---|---|
| `api.health` | GET | `/api/v1/health` | Server, room, client, and event status |
| `api.active-editor` | GET | `/api/v1/editors/active` | Current active-editor snapshot |
| `api.file-rooms` | GET | `/api/v1/files/rooms` | Discover all open file rooms/channels |
| `api.file-room` | GET | `/api/v1/files/rooms/{room}` | Snapshot for one room |
| `api.active-editor-events` | GET | `/api/v1/editors/active/events` | Compatibility SSE stream for the active editor |
| `ws.files` | GET/Upgrade | `/ws/v1/files` | Multiplexed WebSocket; subscribe to one or more rooms |
| `ws.file-room` | GET/Upgrade | `/ws/v1/files/{room}` | Direct WebSocket connection to one file room |

The older `/health`, `/api/editor/snapshot`, and `/api/editor/stream` routes remain as compatibility aliases.

## Authentication

When `auth_token` is empty, authentication is disabled. When it is configured, HTTP and WebSocket requests may authenticate with either:

```text
Authorization: Bearer <token>
```

or:

```text
X-DoDev-Token: <token>
```

Browser WebSocket clients cannot normally add arbitrary request headers, so the WebSocket upgrade also accepts:

```text
ws://127.0.0.1:9934/ws/v1/files?token=<token>
```

DoDevEditor never writes the configured token value to plugin logs. Avoid placing token-bearing URLs in external proxy/access logs.

## File rooms / channels

Each open `EditorPage` is represented by one room. `room` and `channel` are synonyms in the protocol.

Path-backed files use a stable 64-bit FNV-1a identifier for the current DoDevEditor session and path:

```text
file:e3dcdda0dc82b41e
```

Untitled buffers use a session-local identifier:

```text
untitled:00007f2a31b7c010
```

Discover room IDs instead of calculating them yourself:

```bash
curl -H "Authorization: Bearer $TOKEN" \
  http://127.0.0.1:9934/api/v1/files/rooms
```

Example:

```json
{
  "rooms": [
    {
      "room": "file:e3dcdda0dc82b41e",
      "channel": "file:e3dcdda0dc82b41e",
      "title": "main.cpp",
      "path": "/home/user/project/main.cpp",
      "active": true,
      "generation": 12,
      "websocket_path": "/ws/v1/files/file:e3dcdda0dc82b41e"
    }
  ],
  "count": 1
}
```

A room receives events from the exact editor that changed, not only from the currently active tab.

## Snapshot format

File snapshots contain the current unsaved editor buffer:

```json
{
  "room": "file:e3dcdda0dc82b41e",
  "open": true,
  "generation": 12,
  "title": "main.cpp",
  "path": "/home/user/project/main.cpp",
  "caret": 32,
  "selection": { "start": 32, "end": 32 },
  "truncated": false,
  "text": "int main() { return 42; }\n"
}
```

`max_text_bytes` limits the cached text included in network messages. `truncated=true` indicates that the editor buffer was larger than that limit.

## WebSocket protocol

The server implements RFC 6455 directly. Client frames must be masked. Text, close, ping, and pong frames are supported. Fragmented client messages are rejected. Client control JSON is limited to 64 KiB.

### Multiplexed connection

Connect once:

```text
ws://127.0.0.1:9934/ws/v1/files
```

The first server message is:

```json
{
  "type": "session.ready",
  "data": {
    "protocol": "dodev.files.v1",
    "client_id": 7,
    "rooms": {
      "rooms": [],
      "count": 0
    }
  }
}
```

### Subscribe to one room

Client -> server:

```json
{
  "type": "room.subscribe",
  "room": "file:e3dcdda0dc82b41e"
}
```

Server -> client:

```json
{
  "type": "room.subscribed",
  "room": "file:e3dcdda0dc82b41e",
  "channel": "file:e3dcdda0dc82b41e",
  "data": {}
}
```

The server then immediately sends `file.snapshot` for that room.

### Subscribe to all file rooms

```json
{
  "type": "room.subscribe",
  "room": "*"
}
```

The server acknowledges the wildcard and sends the current snapshot of every open room. Future file events for all rooms are then delivered on the same WebSocket.

### Unsubscribe

```json
{
  "type": "room.unsubscribe",
  "room": "file:e3dcdda0dc82b41e"
}
```

### Request the current room list

```json
{
  "type": "room.list"
}
```

Response type: `room.list`.

### File events

A changed editor produces:

```json
{
  "type": "file.changed",
  "room": "file:e3dcdda0dc82b41e",
  "channel": "file:e3dcdda0dc82b41e",
  "data": {
    "room": "file:e3dcdda0dc82b41e",
    "open": true,
    "generation": 13,
    "title": "main.cpp",
    "path": "/home/user/project/main.cpp",
    "caret": 41,
    "selection": { "start": 41, "end": 41 },
    "truncated": false,
    "text": "int main() { return 43; }\n"
  }
}
```

Other event types are:

- `file.opened`
- `file.snapshot`
- `file.changed`
- `file.closed`
- `editor.active_changed` (delivered to wildcard subscribers)
- `stream.resync_required` when a very slow client falls behind the retained event history
- `room.subscribed`
- `room.unsubscribed`
- `room.list`
- `error`
- `pong`

### Direct room connection

A client that only needs one file may connect directly:

```text
ws://127.0.0.1:9934/ws/v1/files/file:e3dcdda0dc82b41e
```

The connection receives `session.ready`, then `file.snapshot`, and then change/open/close events for that room without a separate subscribe message.

### Browser example

```javascript
const token = "change-me";
const ws = new WebSocket(
  `ws://127.0.0.1:9934/ws/v1/files?token=${encodeURIComponent(token)}`
);

ws.onmessage = (event) => {
  const message = JSON.parse(event.data);
  console.log(message.type, message.room, message.data);

  if (message.type === "session.ready") {
    const rooms = message.data.rooms.rooms;
    if (rooms.length > 0) {
      ws.send(JSON.stringify({
        type: "room.subscribe",
        room: rooms[0].room
      }));
    }
  }
};
```

## Multiple clients

The listener does not use a fixed client array. Each accepted connection gets an independent worker and subscription set. Multiple clients may subscribe to the same room or to different rooms simultaneously. The listener backlog is 64; practical concurrency is bounded by operating-system file-descriptor/thread resources rather than a plugin constant.

On plugin shutdown/reload, all live client sockets are shut down first and all worker threads are joined before the plugin library is unloaded.

## Logging policy

The plugin does not display message boxes. Operational feedback goes through `DoDevHostApi::log`, which DoDevEditor routes to the bottom **Logs** tab and normal wx logging output.

Logged activity includes:

- server start/stop/reload;
- every registered endpoint;
- every HTTP request and response status;
- WebSocket upgrade/connect/disconnect;
- every received WebSocket message (type, room, byte count);
- every sent WebSocket message (type, room, byte count);
- ping/pong/close frames;
- SSE events and keep-alives;
- editor opened/changed/closed/active-tab events;
- protocol and authentication errors.

Editor source text and authentication-token values are intentionally **not duplicated into log lines**. The log records message metadata and byte counts instead.

## Threading model

DoDevEditor editor APIs are read only from plugin callbacks on the UI thread. The plugin copies editor state into mutex-protected room snapshots. HTTP/WebSocket worker threads only read those cached snapshots and never call `wxStyledTextCtrl` or other wxWidgets controls.

DoDevEditor's plugin-log bridge marshals worker-thread log calls back onto the UI thread before writing to the Logs control.
