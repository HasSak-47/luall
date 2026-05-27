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

---@class LyraInputKey
---@field kind "none"|"letter"|"modifier"|"special"
---@field letter string|nil
---@field modifier "shift"|"alt"|"ctrl"|nil
---@field special "up"|"down"|"right"|"left"|"enter"|"tab"|"backspace"|"escape"|"delete"|"insert"|"home"|"end"|"page_up"|"page_down"|"f1"|"f2"|"f3"|"f4"|"f5"|"f6"|"f7"|"f8"|"f9"|"f10"|"f11"|"f12"|nil
---@field modifiers integer
---@field shift boolean
---@field alt boolean
---@field ctrl boolean

---@class LyraPipe
---@field close fun(self: LyraPipe)
---@field read fun(self: LyraPipe): string
---@field write fun(self: LyraPipe, data: string)

---@class LyraFile
---@field close fun(self: LyraFile)
---@field write fun(self: LyraFile, data: string)
---@field read fun(self: LyraFile): string
---@field get_fd fun(self: LyraFile): integer

---@class LyraIoApi
---@field pipe fun(): LyraPipe
---@field stderr fun(): LyraFile
---@field stdout fun(): LyraFile
---@field open fun(path: LyraPath, mode: integer|string): LyraFile

---@alias LyraProcessBindable LyraPipe|LyraFile|table

---@class LyraCommand
---@field reserve_size fun(self: LyraCommand, size: integer)
---@field bind fun(self: LyraCommand, bindable: LyraProcessBindable, kind: integer)
---@field add_arg fun(self: LyraCommand, arg: string)
---@field run fun(self: LyraCommand): integer
---@field wait fun(self: LyraCommand): integer
---@field set_foreground fun(self: LyraCommand, foreground: boolean)
---@field get_foreground fun(self: LyraCommand): boolean

---@class LyraProcessApi
---@field command fun(path: string): LyraCommand
---@field wait fun(pid: integer): integer
---@field NONE integer
---@field READ integer
---@field WRITE integer
---@field ERROR integer

---@class LyraLogApi
---@field log fun(level: string)
---@field error fun(msg: string)
---@field warn fun(msg: string)
---@field debug fun(msg: string)
---@field trace fun(msg: string)

---@alias LyraOnEvent fun(event: "enter"|"exit", cb: fun())|fun(event: "key_input", cb: fun(input: LyraInputKey))

---@class LyraCoreApi
---@field process LyraProcessApi
---@field path LyraPathApi
---@field io LyraIoApi
---@field log LyraLogApi
---@field on_event LyraOnEvent
---@field cd fun(path: string|LyraPath): boolean|nil, string|nil
---@field set_raw_mode fun()
---@field unset_raw_mode fun()
---@field enter_alternate_screen fun()
---@field leave_alternate_screen fun()

---@class LyraCore
---@field api LyraCoreApi
---@field state LyraState

---@class LyraBuiltinApi
---@field cd fun(args: string[]|nil)
---@field lua fun(args: string[])

---@class LyraApi: LyraCoreApi
---@field lang LyraLang
---@field render_input fun(data: string, index: integer)
---@field expand_path fun(path: string|nil): LyraPath
---@field format_path fun(path: LyraPath): string
---@field full_color fun(r: integer, g: integer, b: integer): string
---@field reset_color fun(): string
---@field prompt fun(): string
---@field builtin LyraBuiltinApi

---@class Lyra
---@field core LyraCore
---@field state LyraState
---@field plugin LyraPluginApi
---@field api LyraApi
---@field vars LyraStateVars

---@type Lyra
lyra = lyra
