# DoDevEditor wxWidgets 5.1.6 — Docker Manager

## Setup

Install Docker Engine/CLI and Docker Compose v2 on your platform; verify `docker version` and `docker compose version` as the same user running DoDevEditor. The manager uses the local Docker CLI, existing credential helpers and configured contexts. Docker daemon access can be equivalent to root. Do not weaken socket permissions or use an unauthenticated TCP endpoint. No Docker service is started, elevated or reconfigured automatically.

Build with `DODEV_BUILD_PLUGIN_EXAMPLES=ON` (default). The native `com.dodev.docker` plugin is now copied to `build/plugins/docker` and installed to `${CMAKE_INSTALL_BINDIR}/plugins/docker`. In Settings > Plugins enable Docker Manager and restart; use Plugins > Docker Manager. Older command shortcuts remain available. The native view uses the active application theme. It is a modal manager; commands that use the terminal close the manager before starting a new PTY session. Reopen and refresh the inventory to see changes. Read-only queries show output in the manager, with export available; inspect output can contain secrets.

## Features

- Images: list/select, inspect, history, run, pull, push, tag, remove, build a Dockerfile with a chosen context, save/load image archives, import a root filesystem archive, export a derived Dockerfile.
- Containers: list all, inspect, top, start, stop, restart, pause/unpause, kill, remove, exec interactive, attach, start attached, logs follow, live stats, update supported mutable properties, commit, rootfs export and copy files.
- Networks and volumes: list, inspect, create, remove; connect/disconnect container networks.
- Compose: select/edit Compose YAML, up/down/build/ps/logs/config/restart. Advanced operations are available via literal-argument CLI input.
- Configuration: Docker binary, context or daemon URL, CLI config directory, additional global flags (including TLS options), version/info/context list/disk usage, open an existing daemon.json in the editor. Remote configuration files are not automatically fetched; editing a local file does not restart the daemon.
- Run options: image, name, hostname, user, workdir, entrypoint, command/args, environment, published ports, volume and mount specifications, labels, network, restart policy, memory, CPUs, platform, devices, capabilities, interactive/TTY/detached/remove/read-only/privileged toggles. All other Docker flags can be passed as advanced literal args; Docker itself performs final validation. For existing containers, Docker only permits a subset of settings to change via `update`; recreate to change ports or mounts, etc.

## Example

Select alpine:latest after pulling it, or use Run other image. In Run dialog:
- image `alpine:latest`
- name `dodev-alpine`
- interactive and TTY enabled
- command `/bin/sh` (one argument per line)

A real `docker run -it --name dodev-alpine alpine:latest /bin/sh` runs in a dedicated IDE terminal. For a running container use Exec interactive or Attach. Attach uses the existing main process, not a new shell; use Docker's default detach sequence Ctrl+P then Ctrl+Q (unless configured differently). Interrupting a process may stop the container; inspect daemon state after disconnecting. Logs/stats may be interrupted with Ctrl+C. Terminal output can be copied/exported. A stopped container can be started attached with `start -ai`.

Advanced flags and CLI text are one argument per line (not shell syntax). Example run flags:
```
--health-cmd=echo ready
--health-interval=30s
--dns=1.1.1.1
--gpus=all
```
Example Compose/CLI tab for custom command:
```
container
inspect
my-container
```
No shell evaluates these native manager arguments. Empty lines are skipped; literal newlines/empty arguments are not supported in this input format. Values such as passwords should use Docker's normal credential stores/secret facilities, not command-line arguments or saved flags. Inspect can expose environment variables and host paths; export only to a trusted location. Review privileged flags and daemon selection before execution.

## Export distinctions

Dockerfile export generates a **derived** Dockerfile based on an existing image reference (prefers repo digest). The original build instructions, source tree and build secrets cannot be reconstructed from an image. Untagged images need a tag first. The selected image must be available to the builder/registry. Docker image save/load retains image layers and metadata, while container export/image import transfers only the flattened root filesystem (not volume contents, container runtime config or original image history). Use Compose to reproduce runtime settings.

## Validation and limitations

Targeted headless Debug tests: `docker_tools`, `clangformat_docker_plugin_integration`, `wx_raven_menu_parity`. The query test runs a simulated CLI through the actual PTY and checks JSON output, nonzero exit and cancellation. Native wxWidgets GUI compilation and live Docker operations are unverified here: wxWidgets development headers and a Docker daemon are not available. The query path uses PTY output with stdout/stderr combined, so extra CLI warnings can cause structured JSON parsing to fail; use the CLI terminal for those cases. Windows/macOS native behavior is unverified. There is no dedicated graphical form for every CLI option, Docker Desktop setting, Swarm/Buildx extension, or daemon configuration field; use the advanced CLI or edit configuration. No automatic inventory polling, profile persistence for run forms, or direct engine REST API is included.
