# rewsh

`rewsh` is an interactive Linux shell written in C with an embedded Lua runtime.
It is built around a small native core and a plugin-driven frontend: terminal I/O, process execution, userdata, and plugin loading live in the core, while prompt rendering, input handling, and parsing live in plugins.

## Highlights

- Minimal shell engine
- Embedded Lua runtime for shell customization
- Plugin-driven shell behavior instead of hardcoded frontend logic
- Hybrid C/Rust/Lua design
- URL-addressed plugins with dependency resolution
- Private namespaced Lua module loading for plugins
- Runtime process, path, and state APIs exposed to Lua

## Architecture

- The shell loop and terminal input live in C.
- Plugin manifests and URL resolution live in Rust.
- `core` is a C plugin that installs the low-level runtime API into Lua.
- `api` is a Lua plugin that builds the shell-facing API on top of `core`.

The bootstrap config in [config/init.lua](./config/init.lua) loads `core://api`, which pulls in `core://runtime` first and then installs the frontend layer.

## Project Layout

- [src/main.c](./src/main.c): shell loop and key decoding
- [src/state.c](./src/state.c): Lua state setup and plugin loading
- [plugins/runtime](./plugins/runtime): C plugin that exposes the runtime API
- [plugins/api](./plugins/api): Lua plugin that owns shell behavior
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

Each plugin has a `rewsh.toml` manifest and is addressed by URL.

- `core://name` currently resolves to `./plugins/name`
- `path:///foo/bar` resolves to `/foo/bar`

There are two plugin kinds: C plugins loaded as shared objects, and Lua plugins loaded from `init.lua`.

The lifecycle is simple:

- `resolve` reads the manifest and dependency graph
- `load` prepares the plugin handler
- `setup` stores options for later activation
- `require` activates the plugin and returns its exported table

Lua plugins must return a table from `init.lua`. They may also expose optional `setup(opts)` and `unload()` hooks.

Plugin-local Lua modules are private to the plugin that owns them. A plugin can load modules from:

- `<plugin>/init.lua`
- `<plugin>/lua/*.lua`
- `<plugin>/lua/*/init.lua`
- `<plugin>/*.so`

## Runtime API

The shell exposes a global `rewsh` table to Lua. Before core plugins load, it is mostly a bootstrap surface for plugin management.

- `rewsh.plugin.resolve(url)`
- `rewsh.plugin.load(url)`
- `rewsh.plugin.setup(url, opts?)`
- `rewsh.plugin.require(url, opts?)`
- `rewsh.plugin.destroy(url)`

### Core plugins

`core://runtime` is the low-level runtime plugin. Requiring it opens the Lua standard libraries used by the shell, creates `rewsh.core`, and installs the C-backed runtime API for:

- `rewsh.core.api.process`: create commands and pipes
- `rewsh.core.api.path`: parse and build `Path` values
- `rewsh.core.api.on_event(...)`: bind enter/exit/key_input hooks
- `rewsh.core.api.cd(...)`: change cwd from Lua
- `rewsh.core.state`: mutable runtime state

`core://api` is the higher-level shell plugin. It depends on `core://runtime`, then installs the frontend shell layer:

- `rewsh.api`: the shell-facing API table
- `rewsh.api.parser`: the frontend parser
- `rewsh.api.prompt(...)` and `rewsh.api.render_input(...)`: prompt and input rendering helpers
- `rewsh.api.expand_path(...)` and `rewsh.api.format_path(...)`: path helpers built on top of `rewsh.core`
- `rewsh.vars`: a Lua-facing view over `rewsh.core.state.vars`

`rewsh.api` falls back to `rewsh.core.api`, so higher-level helpers and lower-level runtime primitives are available through the same frontend namespace.

## Current Limitations

- No syntax highlighting
- No tab completion
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
