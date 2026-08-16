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

---@param value integer
---@param min integer
---@param max integer
---@return integer
local function clamp(value, min, max)
    if value < min then
        return min
    end

    if value > max then
        return max
    end

    return value
end

---@param text string
---@return string
local function strip_ansi(text)
    return text:gsub("\27%[[%d;]*[A-Za-z]", "")
end

---@return integer
local function terminal_columns()
    local ok, cols = pcall(function()
        return lyra.vars.term.winsize.col
    end)

    if not ok or cols == nil or cols < 1 then
        return 80
    end

    return cols
end

---@param width integer
---@param cols integer
---@return integer
local function screen_row(width, cols)
    return math.floor(width / cols)
end

---@param width integer
---@param cols integer
---@return integer
local function screen_col(width, cols)
    return width % cols
end

---@param width integer
---@param cols integer
---@return integer
local function screen_rows(width, cols)
    return math.floor(width / cols) + 1
end

---@param text string
---@param cols integer
---@return string
local function wrap_ansi(text, cols)
    local rendered = {}
    local visible_col = 0
    local index = 1

    while index <= #text do
        local byte = text:byte(index)
        if byte == 27 and text:sub(index + 1, index + 1) == "[" then
            local _, finish = text:find("\27%[[%d;]*[A-Za-z]", index)
            if finish ~= nil then
                table.insert(rendered, text:sub(index, finish))
                index = finish + 1
            else
                table.insert(rendered, text:sub(index, index))
                visible_col = visible_col + 1
                index = index + 1
            end
        else
            if visible_col == cols then
                table.insert(rendered, "\r\n")
                visible_col = 0
            end

            table.insert(rendered, text:sub(index, index))
            visible_col = visible_col + 1
            index = index + 1
        end
    end

    if visible_col == cols then
        table.insert(rendered, "\r\n")
    end

    return table.concat(rendered)
end

local color_names = {
    black   = "\27[30m",
    red     = "\27[31m",
    green   = "\27[32m",
    yellow  = "\27[33m",
    blue    = "\27[34m",
    magenta = "\27[35m",
    cyan    = "\27[36m",
    white   = "\27[37m",
    reset   = "\27[0m",

    tokens  = {
        ["string"] = full_color(0xbf, 0x9c, 0x30),
        ["identifier"] = full_color(0xff, 0x94, 0x00),
        ["pipe"] = full_color(0x00, 0xa6, 0xb2),
        ["redir"] = full_color(0xbf, 0x5b, 0x30),
    },
}

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

---@param src string
---@param tokens LyraFrontToken[]
local function format_tokens(src, tokens)
    local tcols = color_names.tokens
    local reset = color_names.reset
    local offset = 0

    for _, token in pairs(tokens) do
        local col = tcols[token.type]
        if col ~= nil then
            local prefix = src:sub(1, token.span[1] + offset - 1)
            local postfix = src:sub(1 + token.span[2] + offset, #src)

            src = prefix .. col .. src:sub(token.span[1] + offset, token.span[2] + offset) .. reset .. postfix

            offset = offset + #col + #reset
        end
    end

    return src
end

local last_render_width = 0
local last_render_cursor_width = 0

---@return nil
local function render_input(data, index)
    index = clamp(index, 0, data:len())

    local cols = terminal_columns()
    local prompt = lyra.api.prompt()
    local prompt_width = strip_ansi(prompt):len()
    local visible_width = prompt_width + data:len()
    local rows = screen_rows(visible_width, cols)
    local cursor_width = prompt_width + index
    local cursor_row = screen_row(cursor_width, cols)
    local cursor_col = screen_col(cursor_width, cols)
    local end_row = screen_row(visible_width, cols)
    local last_rows = screen_rows(last_render_width, cols)
    local last_cursor_row = screen_row(last_render_cursor_width, cols)
    local clear_rows = math.max(last_rows, rows)

    if last_cursor_row > 0 then
        io.stdout:write(string.format("\r\27[%dA", last_cursor_row))
    else
        io.stdout:write("\r")
    end

    for row = 1, clear_rows do
        io.stdout:write("\27[2K")
        if row < clear_rows then
            io.stdout:write("\27[1B\r")
        end
    end

    if clear_rows > 1 then
        io.stdout:write(string.format("\r\27[%dA", clear_rows - 1))
    else
        io.stdout:write("\r")
    end

    io.stdout:write(wrap_ansi(prompt .. format_tokens(data, lyra.api.lang.tokenize(data)), cols))
    io.stdout:write("\27[K")

    if end_row > cursor_row then
        io.stdout:write(string.format("\27[%dA", end_row - cursor_row))
    end

    io.stdout:write("\r")
    if cursor_col > 0 then
        io.stdout:write(string.format("\27[%dC", cursor_col))
    end

    last_render_width = visible_width
    last_render_cursor_width = cursor_width
    io.stdout:flush()
end

---@return nil
---@return nil
local function setup_extension_namespace()
    lyra.api = setmetatable({
        lang = lang,
        render_input = render_input,
        format_tokens = format_tokens,
        expand_path = expand_path,
        format_path = format_path,
        color = {
            full_color = full_color,
            reset_color = reset_color,
        },
        reload = function()
            lyra.core.state.reload = true
        end,
        exit = function()
            lyra.core.state.is_running = false
        end,
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
    lyra.vars = setmetatable({
        color_names = color_names
    }, {
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
    end
}
