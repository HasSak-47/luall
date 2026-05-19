---@meta

---@class LyraPluginData

---@class LyraPluginExports: table
---@alias LyraPluginConfig table<string, table>

---@class LyraPluginApi
---@field resolve fun(url: string): LyraPluginData|nil
---@field prepare fun(url: string): LyraPluginData|nil
---@field config LyraPluginConfig
---@field require fun(url: string, opts: table|nil): LyraPluginExports
---@field destroy fun(url: string): nil

---@class LyraStateVarsUser
---@field home LyraPath
---@field name string

---@class LyraStateVars
---@field error integer
---@field debug boolean
---@field env table<string, string>
---@field cwd LyraPath
---@field host string
---@field user LyraStateVarsUser

---@class LyraState
---@field is_running boolean
---@field reload boolean
---@field vars LyraStateVars

---@class LyraPath
---@field push fun(self: LyraPath, name: string)
---@field pop fun(self: LyraPath)
---@field to_string fun(self: LyraPath): string
---@field path_is_dir fun(self: LyraPath): boolean
---@field expand_path fun(self: LyraPath, cwd: LyraPath)
---@field get_childs fun(self: LyraPath): LyraPath[]
---@field get_name fun(self: LyraPath): string

---@class LyraPathApi
---@field new fun(): LyraPath
---@field parse fun(path: string): LyraPath

---@class LyraCommand
---@field add_arg fun(self: LyraCommand, arg: string)
---@field bind_pipe fun(self: LyraCommand, pipe: LyraPipe, bind: integer|string)
---@field reserve_size fun(self: LyraCommand, size: integer)
---@field run fun(self: LyraCommand): integer
---@field set_foreground fun(self: LyraCommand, foreground: boolean)
---@field get_foreground fun(self: LyraCommand): boolean
---@field wait fun(self: LyraCommand): integer

---@class LyraPipe
---@field close fun(self: LyraPipe)
---@field read fun(self: LyraPipe): string
---@field write fun(self: LyraPipe, data: string)

---@class LyraProcessApi
---@field command fun(path: string): LyraCommand
---@field pipe fun(): LyraPipe
---@field wait fun(pid: integer): integer
---@field NONE integer
---@field READ integer
---@field WRITE integer
---@field ERROR integer

---@alias LyraInputKeyKind "none"|"letter"|"modifier"|"special"
---@alias LyraInputModifier "shift"|"alt"|"ctrl"
---@alias LyraInputSpecialKey "up" | "down" | "right" | "left" | "enter" | "tab" | "backspace" | "escape" | "delete" | "insert" | "home" | "end" | "page_up" | "page_down" | "f1" | "f2" | "f3" | "f4" | "f5" | "f6" | "f7" | "f8" | "f9" | "f10" | "f11" | "f12"

---@class LyraInputKey
---@field kind LyraInputKeyKind
---@field letter string|nil
---@field modifier LyraInputModifier|nil
---@field special LyraInputSpecialKey|nil
---@field modifiers integer
---@field shift boolean
---@field alt boolean
---@field ctrl boolean

---@alias LyraEventName "key_input"|"enter"|"exit"
---@alias LyraOnEvent  fun(event: "enter"|"exit", cb: fun()) | fun(event: "key_input", cb: fun(input: LyraInputKey))

---@class LyraCoreApi
---@field process LyraProcessApi
---@field path LyraPathApi
---@field on_event LyraOnEvent
---@field cd fun(path: string|LyraPath): boolean|nil, string|nil
---@field set_raw_mode fun()
---@field unset_raw_mode fun()
---@field enter_alternate_screen fun()
---@field leave_alternate_screen fun()

---@class LyraCore
---@field api LyraCoreApi
---@field state LyraState

---@class Lyra
---@field core LyraCore
---@field state LyraState
---@field plugin LyraPluginApi

---@type Lyra
lyra = lyra
