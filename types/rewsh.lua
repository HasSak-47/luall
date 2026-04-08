---@meta

---@class RewshCorePlugin
---@field setup fun(self: RewshCorePlugin)

---@class RewshPluginApi
---@field load fun(path: string): RewshCorePlugin
---
---@class RewshStateVarsUser
---@field home RewshPath
---@field name string
---
---@class RewshStateVars
---@field error integer
---@field debug boolean
---@field env { [string]: string }
---@field cwd RewshPath
---@field host string
---@field user RewshStateVarsUser

---@class RewshState
---@field running boolean
---@field reload boolean
---@field vars RewshStateVars

---@class RewshPath
---@field push fun(self: RewshPath, name: string)
---@field pop fun(self: RewshPath)
---@field to_string fun(self: RewshPath): string
---@field path_is_dir fun(self: RewshPath): boolean
---@field expand_path fun(self: RewshPath, cwd: RewshPath)
---@field get_childs fun(self: RewshPath): RewshPath[]
---@field get_name fun(self: RewshPath): string

---@class RewshPathApi
---@field new fun(): RewshPath
---@field parse fun(path: string): RewshPath

---@class RewshCommand
---@field add_arg fun(self: RewshCommand, arg: string)
---@field bind_pipe fun(self: RewshCommand, pipe: RewshPipe, bind: integer|string)
---@field reserve_size fun(self: RewshCommand, size: integer)
---@field run fun(self: RewshCommand): integer
---@field set_foreground fun(self: RewshCommand, foreground: boolean)
---@field get_foreground fun(self: RewshCommand): boolean
---@field wait fun(self: RewshCommand): integer

---@class RewshPipe
---@field close fun(self: RewshPipe)

---@class RewshProcessApi
---@field command fun(path: string): RewshCommand
---@field pipe fun(): RewshPipe
---@field wait fun(pid: integer): integer
---@field NONE integer
---@field READ integer
---@field WRITE integer
---@field ERROR integer

---@alias RewshInputKeyKind "none"|"letter"|"modifier"|"special"
---@alias RewshInputModifier "shift"|"alt"|"ctrl"
---@alias RewshInputSpecialKey "up" | "down" | "right" | "left" | "enter" | "tab" | "backspace" | "escape" | "delete" | "insert" | "home" | "end" | "page_up" | "page_down" | "f1" | "f2" | "f3" | "f4" | "f5" | "f6" | "f7" | "f8" | "f9" | "f10" | "f11" | "f12"

---@class RewshInputKey
---@field kind RewshInputKeyKind
---@field letter string|nil
---@field modifier RewshInputModifier|nil
---@field special RewshInputSpecialKey|nil
---@field modifiers integer
---@field shift boolean
---@field alt boolean
---@field ctrl boolean

---@alias RewshEventName "key_input"|"enter"|"exit"
---@alias RewshOnEvent fun(event: "enter"|"exit", cb: fun()) | fun(event: "key_input", cb: fun(input: RewshInputKey))

---@class RewshApi
---@field process RewshProcessApi
---@field path RewshPathApi
---@field on_event RewshOnEvent
---@field cwd fun(path: string|RewshPath): boolean|nil, string|nil
---@field cd fun(path: string|RewshPath): boolean|nil, string|nil
---@field set_raw_mode fun()
---@field unset_raw_mode fun()
---@field enter_alternate_screen fun()
---@field leave_alternate_screen fun()

---@class Rewsh
---@field api RewshApi
---@field state RewshState
---@field plugin RewshPluginApi

---@type Rewsh
rewsh = rewsh
