local lang = require("api.lang")

---@return string
local function home_string()
    return lyra.core.state.vars.user.home:to_string()
end

---@param path string|nil
---@return LyraPath
local function expand_path(path)
    if path == nil or path == "" then
        path = "."
    end

    local parsed = lyra.api.path.parse(path)

    parsed:expand_path(lyra.vars.cwd)
    return parsed
end


---@param path LyraPath
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
        if lyra.vars.error ~= 0 then
            err = " [" .. lyra.vars.error .. "]"
        end

        local debug_prefix = ""
        if lyra.vars.debug then
            debug_prefix = "[DEBUG]"
        end

        local cwd = format_path(lyra.vars.cwd)
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

        return debug_prefix .. "lyra " .. color_text(cwd, 32, 255, 64) .. err
            .. color_text(">", 255, 255, 128)
    end)

    return value
end

---@return nil
local function render_input(data, index)
    local line = "\r" .. lyra.api.prompt() .. data
    io.stdout:write(line)
    io.stdout:write("\27[K")

    local step_back = data:len() - index
    if step_back > 0 then
        io.stdout:write(string.format("\27[%dD", step_back))
    end

    io.stdout:flush()
end

---@return nil
---@return nil
local function setup_extension_namespace()
    lyra.api = setmetatable({
        lang = lang,
        render_input = render_input,
        expand_path = expand_path,
        format_path = format_path,
        full_color = full_color,
        reset_color = reset_color,
        prompt = prompt,
        builtin = {
            cd = function(args)
                local target = nil
                if args == nil or #args == 0 then
                    target = lyra.core.state.vars.user.home
                else
                    target = expand_path(args[1])
                end

                local ok, err = lyra.core.api.cd(target)
                if not ok and err ~= nil then
                    lyra.core.api.log.debug(err)
                end
            end,

            lua = function(args)
                if #args == 0 then
                    return
                end

                local func = load(args[1])
                if func ~= nil then
                    local ok, obj = pcall(func)
                    if not ok then
                        print("failed to run lua code: " .. args[1] .. '\n' .. obj)
                    end
                end
            end,
        }
    }, { __index = lyra.core.api })
    lyra.vars = setmetatable({}, {
        __index = function(_, name)
            return lyra.core.state.vars[name]
        end,
        __newindex = function(_, name, val)
            lyra.core.state.vars[name] = val
        end,
    })
end

return {
    ---@return nil
    setup = function()
        setup_extension_namespace()
    end,
}
