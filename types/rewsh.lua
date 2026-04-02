---@meta

---@class RewshCorePlugin
---@field setup fun(self: RewshCorePlugin)

---@class RewshPluginApi
---@field load fun(path: string): RewshCorePlugin

---@class RewshState
---@field running boolean
---@field reload boolean

---@class RewshPath
---@field push fun(self: RewshPath, name: string)
---@field to_string fun(self: RewshPath): string

---@class RewshPathApi
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

---@class RewshApi
---@field process RewshProcessApi
---@field path RewshPathApi
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
