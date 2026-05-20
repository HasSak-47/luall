---@param values string[]
---@return table<string, boolean>
local function setify_table(values)
    local result = {}
    for _, value in ipairs(values) do
        result[value] = true
    end
    return result
end

local pipe_set = setify_table({ "|", "|&" })
local redir_set = setify_table({ "<", "<<", ">", ">>" })
local fd_set = setify_table({ "&0", "&1", "&2" })

---@param path string
---@return LyraPath
local function expand_front_path(path)
    return lyra.api.expand_path(path)
end

---@class LyraFrontToken
---@field span integer[]
---@field val string
---@field type string

---@class LyraFrontProcess
---@field val LyraFrontToken[]
---@field type string

---@param tokens LyraFrontToken[]
---@return LyraFrontProcess
local function take_process(tokens)
    local limit = 0
    for index, token in ipairs(tokens) do
        if pipe_set[token.val] or redir_set[token.val] or fd_set[token.val] then
            limit = index
            break
        end
    end

    if limit == 0 then
        limit = #tokens
    else
        limit = limit - 1
    end

    local process = {}
    for _ = 1, limit do
        local token = table.remove(tokens, 1)
        token.type = "argument"
        table.insert(process, token)
    end

    if process[1] ~= nil then
        process[1].type = "command"
    end

    return { val = process, type = "process" }
end

---@param input string
---@return table[]
local function tokenize(input)
    local index = 1
    local len = #input
    local tokens = {}

    while index <= len do
        local char = input:sub(index, index)

        if char == '"' or char == "'" then
            local quote = char
            local start = index
            index = index + 1
            while index <= len and input:sub(index, index) ~= quote do
                index = index + 1
            end
            if index <= len then
                index = index + 1
            end
            table.insert(tokens, {
                span = { start, index - 1 },
                val = input:sub(start + 1, index - 2),
                type = "str",
            })
        elseif not char:match("%s") then
            local start = index
            while index <= len and not input:sub(index, index):match("%s") do
                index = index + 1
            end
            table.insert(tokens, {
                span = { start, index - 1 },
                val = input:sub(start, index - 1),
                type = "undefined",
            })
        else
            index = index + 1
        end
    end

    for _, token in ipairs(tokens) do
        if pipe_set[token.val] then
            token.type = "pipe"
        elseif redir_set[token.val] then
            token.type = "redir"
        elseif fd_set[token.val] then
            token.type = "fd"
        else
            token.type = "str"
        end
    end

    local statement = { val = { take_process(tokens) }, type = "statement" }

    while #tokens > 0 do
        if tokens[1].type == "pipe" then
            table.insert(statement.val, table.remove(tokens, 1))
            table.insert(statement.val, take_process(tokens))
        elseif tokens[1].type == "redir" then
            table.insert(statement.val, table.remove(tokens, 1))
            table.insert(statement.val, table.remove(tokens, 1))
        else
            table.remove(tokens, 1)
        end
    end

    return { statement }
end

---@param value string|nil
---@return string[]
local function parse_env_path(value)
    local result = {}
    if value == nil or value == "" then
        return result
    end

    for part in string.gmatch(value, "([^:]+)") do
        table.insert(result, part)
    end
    return result
end

---@param name string
---@return string
local function resolve_command(name)
    if name:find("/", 1, true) ~= nil then
        return expand_front_path(name):to_string()
    end

    local path_env = lyra.core.state.vars.env.PATH
    for _, path in ipairs(parse_env_path(path_env)) do
        local candidate = path .. "/" .. name
        local handle = io.open(candidate, "r")
        if handle ~= nil then
            handle:close()
            return candidate
        end
    end

    return name
end

---@param process LyraFrontProcess
---@return string, string[]
local function build_command(process)
    local name = table.remove(process.val, 1).val
    local args = {}
    for _, token in ipairs(process.val) do
        table.insert(args, token.val)
    end

    return name, args
end

---@param process LyraFrontProcess
---@return LyraCommand, string, string[]
local function make_command(process)
    local name, args = build_command(process)
    local target = resolve_command(name)
    local command = lyra.api.process.command(target)
    command:reserve_size(#args + 1)

    for _, arg in ipairs(args) do
        command:add_arg(arg)
    end

    return command, name, args
end

---@param redir LyraFrontToken
---@param file LyraFrontToken
---@return LyraFile
local function open_redirect(redir, file)
    if redir.val ~= ">" and redir.val ~= ">>" then
        error("only > and >> redirection is supported")
    end

    local mode = redir.val == ">>" and "a" or "w"
    return lyra.api.io.open(expand_front_path(file.val), mode)
end

---@param command LyraCommand
---@param output LyraFile|nil
---@return integer
local function run_command(command, output)
    if output ~= nil then
        command:bind(output, lyra.api.process.WRITE)
    end

    command:run()
    if output ~= nil then
        output:close()
    end
    local status = command:wait()
    return status
end

---@param process LyraFrontProcess
---@return nil
local function run_cmd(process)
    local command, name, args = make_command(process)

    if name == "exit" then
        lyra.core.state.is_running = false
        lyra.core.state.vars.error = 0
        return
    end

    if name == "reload" then
        lyra.core.state.reload = true
        lyra.core.state.vars.error = 0
        return
    end

    if lyra.api.builtin[name] then
        lyra.api.builtin[name](args)
        return
    end

    lyra.vars.error = run_command(command, nil)
end

---@param process LyraFrontProcess
---@param redir LyraFrontToken
---@param file LyraFrontToken
---@return nil
local function run_redirected(process, redir, file)
    local command, name = make_command(process)
    if lyra.api.builtin[name] then
        error("redirection is only supported for external commands")
    end

    local output = open_redirect(redir, file)
    lyra.vars.error = run_command(command, output)
end

---@param tokens table[]
---@param redir LyraFrontToken|nil
---@param file LyraFrontToken|nil
---@return nil
local function run_piped(tokens, redir, file)
    local src, src_name = make_command(tokens[1])
    local out, out_name = make_command(tokens[3])
    if lyra.api.builtin[src_name] or lyra.api.builtin[out_name] then
        error("pipes are only supported for external commands")
    end

    local pipe = lyra.api.io.pipe()
    local output = nil
    if redir ~= nil and file ~= nil then
        output = open_redirect(redir, file)
        out:bind(output, lyra.api.process.WRITE)
    end
    if tokens[2].val == "|&" then
        src:bind(pipe, lyra.api.process.WRITE + lyra.api.process.ERROR)
    else
        src:bind(pipe, lyra.api.process.WRITE)
    end
    out:bind(pipe, lyra.api.process.READ)

    src:run()
    out:run()
    pipe:close()
    if output ~= nil then
        output:close()
    end

    local src_status = src:wait()
    local out_status = out:wait()
    lyra.vars.error = out_status ~= 0 and out_status or src_status
end

---@param line string
---@return nil
local function handle_shell_like(line)
    local statement = tokenize(line)[1].val
    if #statement == 1 then
        run_cmd(statement[1])
    elseif #statement == 3 and statement[2].type == "pipe" then
        run_piped(statement)
    elseif #statement == 3 and statement[2].type == "redir" then
        run_redirected(statement[1], statement[2], statement[3])
    elseif #statement == 5 and statement[2].type == "pipe"
        and statement[4].type == "redir" then
        run_piped(statement, statement[4], statement[5])
    else
        error("unsupported shell syntax")
    end
end

---@param line string
---@return nil
local function handle_singleline(line)
    local start = line:find("lua", 1, true)
    if start == 1 then
        lyra.api.builtin.lua({ line:gsub("^lua%s*", "", 1) })
    else
        handle_shell_like(line)
    end
end

---@param input string|nil
---@return nil
local function parse(input)
    if input == nil or input == "" then
        lyra.vars.error = 0
        return
    end

    local set_debug = input:sub(1, 1) == "!"
    if set_debug then
        input = input:sub(2)
        lyra.vars.debug = true
    end

    local lines = {}
    for line in input:gmatch("[^\n]+") do
        if line ~= "" then
            table.insert(lines, line)
        end
    end

    if #lines == 1 then
        handle_singleline(lines[1])
    end

    if set_debug then
        lyra.vars.debug = false
    end
end

return {
    parse = parse,
    tokenize = tokenize,
}
