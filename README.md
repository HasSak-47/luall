# rewsh

`rewsh` is an interactive Linux shell written in C with an embedded Lua runtime.
The shell core owns terminal I/O, process execution, path/state userdata, and plugin loading.
Shell behavior such as prompt rendering, input handling, and command parsing now lives in plugins.

## Current Shape

- The shell loop and input decoding live in C.
- CLI argument parsing and plugin manifest parsing live in Rust.
- `core_back` is a C plugin that exposes the Lua API for:
  - process execution
  - path objects
  - state access
  - event hooks
  - terminal mode helpers
- `core_front` is a Lua plugin that owns:
  - prompt rendering
  - input buffer editing
  - shell parsing
  - aliases and Lua-side builtins
  - session history

The bootstrap config in [config/init.lua](./config/init.lua) simply loads `core_back` and `core_front`.

## Features

- Embedded Lua runtime for shell customization
- Plugin manifests with app-local loading via `app://...`
- Lua plugins and C plugins
- `RewshPath` userdata exposed to Lua
- Process execution and simple pipelines from the frontend parser
- Runtime state exposed to Lua:
  - `rewsh.state.vars.cwd`
  - `rewsh.state.vars.user`
  - `rewsh.state.vars.host`
  - `rewsh.state.vars.env`
  - `rewsh.state.vars.error`
  - `rewsh.state.vars.debug`
- Plugin-relative Lua module resolution for Lua plugins

## Project Layout

- [src/main.c](./src/main.c): shell loop and key decoding
- [src/state.c](./src/state.c): Lua state setup and plugin loading
- [plugins/core_back](./plugins/core_back): backend Lua API plugin
- [plugins/core_front](./plugins/core_front): frontend shell plugin
- [types/rewsh.lua](./types/rewsh.lua): Lua type stubs for the exposed API

## Building

### Dependencies

- Lua 5.4
- gcc
- make
- Rust toolchain (`cargo`, `rustc`)
- `cbindgen`
- `pkg-config`

### Commands

- Build: `make build`
- Run: `make run`
- Clean: `make clean`
- Test build with `LY_TEST`: `make test`

The main binary is `./rewsh`.

## Plugin Model

Each plugin has a `rewsh.toml` manifest.

- C plugins are compiled and loaded as shared objects.
- Lua plugins are loaded from `init.lua`.
- When a Lua plugin is loaded, rewsh prepends these plugin-local search paths so `require(...)` works inside the plugin:
  - `<plugin>/lua/?.lua`
  - `<plugin>/lua/?/init.lua`
  - `<plugin>/?.so`

That lets a Lua plugin use a layout like:

```text
plugins/core_front/
  init.lua
  lua/
    parser.lua
```

## Runtime API

The shell exposes a global `rewsh` table to Lua.

- `rewsh.plugin.load("app://name")`: load a plugin by app path
- `rewsh.api.process`: create commands and pipes
- `rewsh.api.path`: parse and build `RewshPath` values
- `rewsh.api.on_event(...)`: bind enter/exit/key_input hooks
- `rewsh.api.cd(...)`: change cwd from Lua
- `rewsh.state`: mutable runtime state
- `rewsh.front`: frontend-owned Lua namespace installed by `core_front`

The frontend parser currently lives under `rewsh.front.api`.

## Current Limitations

- No syntax highlighting
- No tab completion
- No history navigation UI yet
- No job control (`fg`, `bg`)
- No glob expansion
- Redirection tokens are parsed but not executed yet
- Multiline command handling is still incomplete
- Shell behavior assumes a real TTY; non-interactive runs are still rough
- The Lua/C API is thin and unsafe; bad userdata or bad assumptions can crash the shell

## Notes

- `Ctrl+C` handling is still primitive and not yet shell-like.
- Reload behavior exists through `rewsh.state.reload`, but this codebase is still in active transition and not hardened.

## Foot Guns

- Lua can mutate shared global state directly.
- Raw mode is enabled during interactive use; a crash may leave your terminal in a bad state.
- Plugin code runs with very little protection.
- Path/process/state userdata are fast, but not defensive.
