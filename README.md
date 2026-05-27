# Lyra

`lyra` is an interactive Linux shell written in C with an embedded Lua runtime.
It is built around a small native core and a plugin-driven frontend: terminal I/O, process execution, userdata, and plugin loading live in the core, while prompt rendering, input handling, and parsing live in plugins.

https://github.com/user-attachments/assets/c92d6cd8-56fe-4e29-bc3c-1473fe78469e

## Highlights

- Minimal shell engine
- Embedded Lua runtime for shell customization
- Plugin-driven shell behavior instead of hardcoded frontend logic
- Hybrid C/Rust/Lua design
- URL-addressed plugins with dependency resolution
- Private namespaced Lua module loading for plugins
- Runtime process, path, and state APIs exposed to Lua

## Architecture

- The shell event loop and terminal input live in C.
- Plugin manifests and URL resolution live in Rust.
- `runtime` is a C plugin that installs the low-level runtime API into Lua.
- `api` is a Lua plugin that builds the shell-facing API on top of `runtime`.

The bootstrap config in [config/init.lua](./config/init.lua) currently loads `core://runtime` and `core://api` directly.

## Project Layout

- [src/main.c](./src/main.c): shell loop and key decoding
- [src/state.c](./src/state.c): Lua state setup and plugin loading
- [crates/cli](./crates/cli): Rust CLI parsing and FFI bindings
- [crates/log](./crates/log): Rust logging bridge
- [crates/plugins](./crates/plugins): Rust plugin manifest resolution and loading support
- [crates/lyra](./crates/lyra): Rust shared library linked into the final binary
- [plugins/runtime](./plugins/runtime): C plugin that exposes the runtime API
- [plugins/api](./plugins/api): Lua plugin that owns shell behavior
- [config/init.lua](./config/init.lua): default bootstrap config
- [types/lyra.lua](./types/lyra.lua): Lua type stubs for the exposed API

## Building

These instructions are written for the `stable` branch.

If you are on `main`, you are on the development branch and should expect breakage, incomplete features, or temporary build failures.

### Dependencies

- Lua 5.4
- gcc
- make
- Rust toolchain (`cargo`, `rustc`)
- `cbindgen`
- `pkg-config`

Lua 5.4 must be discoverable through `pkg-config` for the build to succeed.

### Commands

- Build: `make build`
- Run: `make run`
- Clean: `make clean`
- Test build `make test`

`make build` compiles both the C sources and the Rust shared library, then links `./lyra`.

The main binary is `./lyra`.

## Plugin Model

Each plugin has a `lyra.toml` manifest and is addressed by URL.

- `core://name` currently resolves to `./plugins/name`
- `path:///foo/bar` resolves to `/foo/bar`

C plugins are currently compiled into the cache directory before loading.

The manifest supports `Lua`, `C`, `Binary`, and `Rust` plugin kinds. In the current codebase, Lua and C plugins are the main implemented paths, binary plugins can load an existing shared object, and the Rust plugin path is still incomplete.

The lifecycle is simple:

- `resolve` reads the manifest and dependency graph
- `prepare` prepares the plugin handler
- `require` activates the plugin and returns its exported table

Lua plugins must return a table from `init.lua`. They may also expose optional `setup(opts)` and `unload()` hooks.

Plugin-local Lua modules are private to the plugin that owns them. A plugin can load modules from:

- `<plugin>/init.lua`
- `<plugin>/lua/*.lua`
- `<plugin>/lua/*/init.lua`

## Runtime API

The shell exposes a global `lyra` table to Lua. Before core plugins load, it is mostly a bootstrap surface for plugin management.

- `lyra.plugin.resolve(url)`
- `lyra.plugin.prepare(url)`
- `lyra.plugin.require(url, opts?)`
- `lyra.plugin.destroy(url)`

### Core plugins

`core://runtime` is the low-level runtime plugin. Requiring it opens the Lua standard libraries used by the shell, creates `lyra.core`, and installs the C-backed runtime API for:

- `lyra.core.api.process`: create commands and pipes
- `lyra.core.api.path`: parse and build `Path` values
- `lyra.core.api.io`: file and pipe helpers
- `lyra.core.api.log`: runtime logging helpers
- `lyra.core.api.on_event(...)`: bind enter/exit/key_input hooks
- `lyra.core.api.cd(...)`: change cwd from Lua
- `lyra.core.state`: mutable runtime state

When required as a C plugin, `core://runtime` currently returns its setup/config table.

`core://api` is the higher-level shell plugin. It depends on `core://runtime`, then installs the frontend shell layer:

- `lyra.api`: the shell-facing API table
- `lyra.api.lang`: tokenize, parse, and run frontend shell input
- `lyra.api.prompt(...)` and `lyra.api.render_input(...)`: prompt and input rendering helpers
- `lyra.api.expand_path(...)` and `lyra.api.format_path(...)`: path helpers built on top of `lyra.core`
- `lyra.vars`: a Lua-facing view over `lyra.core.state.vars`

`lyra.api` falls back to `lyra.core.api`, so higher-level helpers and lower-level runtime primitives are available through the same frontend namespace.

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
- Reload behavior exists through `lyra.state.reload`, but this codebase is still in active transition and not hardened.
- `--script` is parsed by the CLI, but script execution is not wired up yet.

## Foot Guns

- Lua can mutate shared global state directly.
- Raw mode is enabled during interactive use; a crash may leave your terminal in a bad state.
- Plugin code runs with very little protection.
- Path/process/state userdata are fast, but not defensive.
