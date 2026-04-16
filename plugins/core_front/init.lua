local parser = require("parser")

---@return string
local function home_string()
    return rewsh.state.vars.user.home:to_string()
end

---@param path string|nil
---@return RewshPath
local function expand_path(path)
    if path == nil or path == "" then
        path = "."
    end

    local parsed = rewsh.api.path.parse(path)
    parsed:expand_path(rewsh.state.vars.cwd)
    return parsed
end


---@param path RewshPath
---@return string
local function format_path(path)
    local path_string = path:to_string()

    local home = home_string()
    local start_at = path_string:find(home, 1, true)
    if start_at ~= 1 then
        return path_string
    end

    path_string = path_string:gsub(home, "~", 1)
    return path_string
end

---@param r integer
---@param g integer
---@param b integer
---@return string
local function full_color(r, g, b)
    return "\27[38;2;" .. r .. ";" .. g .. ";" .. b .. "m"
end

---@return string
local function reset_color()
    return "\27[0m"
end

---@return string
local function prompt()
    local ok, value = pcall(function()
        local err = ""
        if rewsh.state.vars.error ~= 0 then
            err = " [" .. rewsh.state.vars.error .. "]"
        end

        local debug_prefix = ""
        if rewsh.state.vars.debug then
            debug_prefix = "[DEBUG]"
        end

        local cwd = format_path(rewsh.state.vars.cwd)
        if cwd == nil then
            cwd = 'FAILED FORMAT'
        end

        ---@param text string
        ---@param r integer
        ---@param g integer
        ---@param b integer
        local function color_text(text, r, g, b)
            return full_color(r, g, b) .. text .. reset_color()
        end

        return debug_prefix .. "rewsh " .. color_text(cwd, 32, 255, 64) .. err
            .. color_text(">", 255, 255, 128)
    end)

    return value
end

---@return nil
local function render_input()
    local front = rewsh.front
    local line = "\r" .. front.prompt() .. front.vars.input.data
    io.stdout:write(line)
    io.stdout:write("\27[K")

    local step_back = front.vars.input.data:len() - front.vars.input.index
    if step_back > 0 then
        io.stdout:write(string.format("\27[%dD", step_back))
    end

    io.stdout:flush()
end

---@return nil
---@return nil
local function setup_front_namespace()
    rewsh.front = rewsh.front or {}
    rewsh.front.api = rewsh.front.api or {}
    rewsh.front.vars = rewsh.front.vars or {}
    rewsh.front.alias = rewsh.front.alias or {}
    rewsh.front.util = rewsh.front.util or {}
    rewsh.front.extend = rewsh.front.extend or {}
    rewsh.front.inner = rewsh.front.inner or {}
    rewsh.front.prompts = rewsh.front.prompts or {}
    rewsh.front.testing = rewsh.front.testing or {}

    rewsh.front.vars.history = rewsh.front.vars.history or {
        'lua print("hello world")',
        "!ls -lA",
    }

    rewsh.front.vars.input = rewsh.front.vars.input or {
        data = "",
        index = 0,
    }

    rewsh.front.inner.expand_path = expand_path
    rewsh.front.inner.format_path = format_path
    rewsh.front.inner.full_color = full_color
    rewsh.front.inner.reset_color = reset_color
    rewsh.front.prompt = prompt
    rewsh.front.prompts.prompt = prompt
    rewsh.front.render_input = render_input
    rewsh.front.api.parse = parser.parse
    rewsh.front.api.tokenize = parser.tokenize
end

---@return nil
local function setup_builtins()
    rewsh.front.alias.ls = function(args)
        return "ls", { "--color", table.unpack(args) }
    end

    rewsh.front.alias.ll = function(args)
        return "ls", { "--color", "-lA", table.unpack(args) }
    end

    rewsh.front.util.lua = function(args)
        if #args == 0 then
            return
        end

        local func = load(args[1])
        if func ~= nil then
            local ok = pcall(func)
            if not ok then
                print("failed to run")
            end
        end
    end

    rewsh.front.util.alias = function(args)
        local name = table.remove(args, 1)
        if name == nil then
            return
        end

        local func = load(args[1] or "")
        if func == nil then
            print("could not create alias!")
            return
        end

        rewsh.front.alias[name] = func
    end

    rewsh.front.extend.cd = function(args)
        local target = nil
        if args == nil or #args == 0 then
            target = rewsh.state.vars.user.home
        else
            target = expand_path(args[1])
        end

        local ok, err = rewsh.api.cd(target)
        if not ok and err ~= nil then
            print(err)
        end
    end
end

return {
    ---@return nil
    setup = function()
        setup_front_namespace()
        setup_builtins()
    end,
}
