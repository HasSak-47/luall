---@meta

---@class LyraPluginData

---@class LyraPluginExports: table

---@alias LyraPluginConfig table<string, table>

---@class LyraPluginApi
---@field resolve fun(url: string): LyraPluginData|nil
---@field prepare fun(url: string): LyraPluginData|nil
---@field require fun(url: string, opts?: table): LyraPluginExports
---@field destroy fun(url: string)
---@field config LyraPluginConfig

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

---@class LyraStateVarsTermWindowSize
---@field row integer
---@field col integer
---@field xpixel integer
---@field ypixel integer

---@class LyraStateVarsTerm
---@field in_raw_mode boolean
---@field in_alternate_screen boolean
---@field winsize LyraStateVarsTermWindowSize

---@class LyraStateVars
---@field error integer
---@field debug boolean
---@field env table<string, string>
---@field cwd LyraPath
---@field host string
---@field user LyraStateVarsUser
---@field term LyraStateVarsTerm

---@class LyraState
---@field is_running boolean
---@field reload boolean
---@field vars LyraStateVars

---@alias LyraInputKeyKind "none"|"letter"|"modifier"|"special"
---@alias LyraInputModifier "shift"|"alt"|"ctrl"
---@alias LyraInputSpecialKey "up"|"down"|"right"|"left"|"enter"|"tab"|"backspace"|"escape"|"delete"|"insert"|"home"|"end"|"page_up"|"page_down"|"f1"|"f2"|"f3"|"f4"|"f5"|"f6"|"f7"|"f8"|"f9"|"f10"|"f11"|"f12"

---@class LyraInputKey
---@field kind LyraInputKeyKind
---@field letter string|nil
---@field modifier LyraInputModifier|nil
---@field special LyraInputSpecialKey|nil
---@field modifiers integer
---@field shift boolean
---@field alt boolean
---@field ctrl boolean

---@alias LyraSignalName "int"|"term"|"cont"|"quit"|"hup"|"chld"|"alrm"|"pipe"|"segv"|"fpe"|"abrt"|"usr1"|"usr2"|"window_change"|"unknown"

---@class LyraSignal
---@field code integer
---@field name LyraSignalName

---@class LyraPipe
---@field close fun(self: LyraPipe)
---@field read fun(self: LyraPipe): string
---@field write fun(self: LyraPipe, data: string)

---@class LyraFile
---@field open fun(self: LyraFile, path: LyraPath, mode: LyraOpenMode)
---@field close fun(self: LyraFile)
---@field write fun(self: LyraFile, data: string)
---@field read fun(self: LyraFile): string
---@field get_fd fun(self: LyraFile): integer

---@alias LyraOpenMode integer|"r"|"w"|"a"|"r+"|"w+"|"a+"

---@class LyraIoApi
---@field pipe fun(): LyraPipe
---@field stderr fun(): LyraFile
---@field stdout fun(): LyraFile
---@field open fun(path: LyraPath, mode: LyraOpenMode): LyraFile

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

---@alias LyraLogLevel "error"|"warn"|"debug"|"trace"|string

---@class LyraLogApi
---@field log fun(level: LyraLogLevel)
---@field error fun(msg: string)
---@field warn fun(msg: string)
---@field debug fun(msg: string)
---@field trace fun(msg: string)

---@alias LyraOnEvent fun(event: "enter"|"exit", cb: fun())|fun(event: "key_input", cb: fun(input: LyraInputKey))|fun(event: "signal", cb: fun(signal: LyraSignal))

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

---@alias LyraFrontTokenType "argument"|"command"|"debug"|"error"|"fd"|"identifier"|"lua_code"|"pipe"|"redir"|"str"|"string"|"undefined"

---@class LyraFrontToken
---@field span integer[]
---@field val string
---@field type LyraFrontTokenType|string
---@field error string|nil

---@class LyraFrontProcess
---@field val LyraFrontToken[]
---@field type "process"

---@class LyraFrontLuaStatement
---@field val string
---@field type "lua"

---@class LyraFrontShellStatement
---@field val table[]
---@field type "statement"

---@alias LyraFrontStatement LyraFrontLuaStatement|LyraFrontShellStatement

---@class LyraFrontChunk
---@field val LyraFrontStatement[]
---@field type "chunk"
---@field debug boolean

---@class LyraLangDebug
---@field print_tokens fun(tokens: LyraFrontToken[])

---@class LyraLang
---@field debug LyraLangDebug
---@field parse fun(tokens?: LyraFrontToken[]): LyraFrontChunk
---@field run fun(tree: LyraFrontChunk|LyraFrontStatement|nil)
---@field tokenize fun(input: string): LyraFrontToken[]

---@class LyraBuiltinApi
---@field cd fun(args?: string[])
---@field lua fun(args: string[])

---@class LyraColorApi
---@field full_color fun(r: integer, g: integer, b: integer): string
---@field reset_color fun(): string

---@class LyraTokenColorNames
---@field string string
---@field identifier string
---@field pipe string
---@field redir string

---@class LyraColorNames
---@field black string
---@field red string
---@field green string
---@field yellow string
---@field blue string
---@field magenta string
---@field cyan string
---@field white string
---@field reset string
---@field tokens LyraTokenColorNames

---@class LyraApi: LyraCoreApi
---@field lang LyraLang
---@field render_input fun(data: string, index: integer)
---@field format_tokens fun(src: string, tokens: LyraFrontToken[]): string
---@field expand_path fun(path?: string): LyraPath
---@field format_path fun(path: LyraPath): string
---@field color LyraColorApi
---@field reload fun()
---@field exit fun()
---@field prompt fun(): string
---@field builtin LyraBuiltinApi

---@class LyraVars: LyraStateVars
---@field color_names LyraColorNames

---@class Lyra
---@field core LyraCore
---@field state LyraState
---@field plugin LyraPluginApi
---@field api LyraApi
---@field vars LyraVars

---@type Lyra
lyra = lyra
