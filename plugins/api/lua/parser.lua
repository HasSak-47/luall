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
---@return RewshPath
local function expand_front_path(path)
    return rewsh.api.expand_path(path)
end

---@class RewshFrontToken
---@field span integer[]
---@field val string
---@field type string

---@class RewshFrontProcess
---@field val RewshFrontToken[]
---@field type string

---@param tokens RewshFrontToken[]
---@return RewshFrontProcess
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

    local path_env = rewsh.core.state.vars.env.PATH
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

---@param process RewshFrontProcess
---@return string, string[]
local function build_command(process)
    local name = table.remove(process.val, 1).val
    local args = {}
    for _, token in ipairs(process.val) do
        table.insert(args, token.val)
    end

    return name, args
end

---@param name string
---@param args string[]
---@return nil
local function run_external(name, args)
    local target = resolve_command(name)
    local command = rewsh.api.process.command(target)
    command:reserve_size(#args + 1)

    for _, arg in ipairs(args) do
        command:add_arg(arg)
    end

    command:run()
    local status = command:wait()
    rewsh.vars.error = status
end

---@param process RewshFrontProcess
---@return nil
local function run_cmd(process)
    local name, args = build_command(process)

    if name == "exit" then
        rewsh.core.state.is_running = false
        rewsh.core.state.vars.error = 0
        return
    end

    if name == "reload" then
        rewsh.core.state.reload = true
        rewsh.core.state.vars.error = 0
        return
    end

    if rewsh.api.builtin[name] then
        rewsh.api.builtin[name](args)
        return
    end

    run_external(name, args)
end

---@param tokens table[]
---@return nil
local function run_piped(tokens)
    local src_name, src_args = build_command(tokens[1])
    local out_name, out_args = build_command(tokens[3])

    local src = rewsh.api.process.command(resolve_command(src_name))
    src:reserve_size(#src_args + 1)
    for _, arg in ipairs(src_args) do
        src:add_arg(arg)
    end

    local out = rewsh.api.process.command(resolve_command(out_name))
    out:reserve_size(#out_args + 1)
    for _, arg in ipairs(out_args) do
        out:add_arg(arg)
    end

    local pipe = rewsh.api.process.pipe()
    src:bind_pipe(pipe, "write")
    if tokens[2].val == "|&" then
        src:bind_pipe(pipe, rewsh.api.process.WRITE + rewsh.api.process.ERROR)
    end
    out:bind_pipe(pipe, "read")

    src:run()
    out:run()
    pipe:close()

    local src_status = src:wait()
    local out_status = out:wait()
    rewsh.state.vars.error = out_status ~= 0 and out_status or src_status
end

---@param line string
---@return nil
local function handle_shell_like(line)
    local tokens = tokenize(line)
    if #tokens[1].val == 1 then
        run_cmd(tokens[1].val[1])
    else
        run_piped(tokens[1].val)
    end
end

---@param line string
---@return nil
local function handle_singleline(line)
    local start = line:find("lua", 1, true)
    if start == 1 then
        rewsh.api.lua({ line:gsub("^lua%s*", "", 1) })
    else
        handle_shell_like(line)
    end
end

---@param input string|nil
---@return nil
local function parse(input)
    if input == nil or input == "" then
        rewsh.vars.error = 0
        return
    end

    local set_debug = input:sub(1, 1) == "!"
    if set_debug then
        input = input:sub(2)
        rewsh.vars.debug = true
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
        rewsh.vars.debug = false
    end
end

return {
    parse = parse,
    tokenize = tokenize,
}
