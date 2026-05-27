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
---@field error string|nil

---@class LyraFrontProcess
---@field val LyraFrontToken[]
---@field type string

---@class LyraFrontStatement
---@field val table[]
---@field type string

---@class LyraFrontChunk
---@field val LyraFrontStatement[]
---@field type "chunk"
---@field debug boolean

---@class LyraLang
---@field parse fun(tokens: LyraFrontToken[]|nil): LyraFrontChunk
---@field run fun(tree: LyraFrontChunk|LyraFrontStatement)
---@field tokenize fun(input: string): LyraFrontToken[]

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
        if token.type ~= "error" then
            token.type = "argument"
        end
        table.insert(process, token)
    end

    if process[1] ~= nil and process[1].type ~= "error" then
        process[1].type = "command"
    end

    return { val = process, type = "process" }
end

---@param input string
---@return LyraFrontToken[]
local function tokenize(input)
    local index = 1
    local len = #input
    local tokens = {}

    if input:sub(1, 1) == "!" then
        table.insert(tokens, {
            span = { 1, 1 },
            val = "!",
            type = "debug",
        })
        index = 2
    end

    local lua_start, lua_end = input:find("lua%s+", index)
    if lua_start == index then
        table.insert(tokens, {
            span = { lua_start, lua_start + 2 },
            val = "lua",
            type = "str",
        })
        table.insert(tokens, {
            span = { lua_end + 1, len },
            val = input:sub(lua_end + 1),
            type = "lua_code",
        })
        return tokens
    end

    while index <= len do
        local char = input:sub(index, index)

        -- get string literals
        if char == '"' or char == "'" then
            local quote = char
            local start = index
            index = index + 1
            while index <= len and input:sub(index, index) ~= quote do
                index = index + 1
            end
            if index > len then
                table.insert(tokens, {
                    span = { start, len },
                    val = input:sub(start),
                    type = "error",
                    error = "unterminated string",
                })
            else
                index = index + 1
                table.insert(tokens, {
                    span = { start, index - 1 },
                    val = input:sub(start + 1, index - 2),
                    type = "string",
                })
            end
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
        if token.type == "error" or token.type == "debug" or token.type == "string" then
            -- keep special lexer tokens intact for callers and parse validation
        elseif pipe_set[token.val] then
            token.type = "pipe"
        elseif redir_set[token.val] then
            token.type = "redir"
        elseif fd_set[token.val] then
            token.type = "fd"
        else
            token.type = "identifier"
        end
    end

    return tokens
end

---@param tokens LyraFrontToken[]
---@return LyraFrontToken[]
local function clone_tokens(tokens)
    local result = {}
    for _, token in ipairs(tokens) do
        table.insert(result, {
            span = { token.span[1], token.span[2] },
            val = token.val,
            type = token.type,
            error = token.error,
        })
    end
    return result
end

---@param tokens LyraFrontToken[]
---@return LyraFrontStatement
local function parse_statement(tokens)
    if tokens[1] ~= nil and tokens[1].type == "str" and tokens[1].val == "lua" then
        local code = ""
        if tokens[2] ~= nil then
            code = tokens[2].val
        end

        return {
            type = "lua",
            val = code,
        }
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

    return statement
end

---@param process LyraFrontProcess|nil
---@param context string
---@return nil
local function validate_process(process, context)
    if process == nil or process.type ~= "process" or #process.val == 0 then
        error("syntax error: expected command " .. context)
    end

    if process.val[1].type ~= "command" or process.val[1].val == "" then
        error("syntax error: expected command " .. context)
    end
end

---@param node table
---@return nil
local function validate_no_error_tokens(node)
    if node.type == "error" or node.error ~= nil then
        error("syntax error: " .. node.error)
    end

    if node.val == nil or type(node.val) ~= "table" then
        return
    end

    for _, child in ipairs(node.val) do
        if type(child) == "table" then
            validate_no_error_tokens(child)
        end
    end
end

---@param statement LyraFrontStatement
---@return nil
local function validate_statement(statement)
    validate_no_error_tokens(statement)

    if statement.type == "lua" then
        return
    end

    if statement == nil or statement.type ~= "statement" then
        error("syntax error: expected statement")
    end

    local nodes = statement.val
    validate_process(nodes[1], "at start of statement")

    if #nodes == 1 then
        return
    end

    if #nodes == 3 and nodes[2].type == "pipe" then
        validate_process(nodes[3], "after pipe")
        return
    end

    if #nodes == 3 and nodes[2].type == "redir" then
        if nodes[3] == nil or not (nodes[3].type == "string" or nodes[3].type == "identifier") or nodes[3].val == "" then
            error("syntax error: expected file after redirection")
        end

        if nodes[2].val ~= ">" and nodes[2].val ~= ">>" then
            error("syntax error: unsupported redirection '" .. nodes[2].val .. "'")
        end
        return
    end

    if #nodes == 5 and nodes[2].type == "pipe" and nodes[4].type == "redir" then
        validate_process(nodes[3], "after pipe")
        if nodes[5] == nil or nodes[5].type ~= "identifier" or nodes[5].val == "" then
            error("syntax error: expected file after redirection")
        end

        if nodes[4].val ~= ">" and nodes[4].val ~= ">>" then
            error("syntax error: unsupported redirection '" .. nodes[4].val .. "'")
        end
        return
    end

    error("syntax error: unsupported shell syntax")
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
    local name = process.val[1].val
    local args = {}
    for i = 2, #process.val do
        local token = process.val[i]
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

---@param statement LyraFrontStatement
---@return nil
local function run_statement(statement)
    statement = statement.val
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

---@param tokens LyraFrontToken[]|nil
---@return LyraFrontChunk
local function parse(tokens)
    if tokens == nil or #tokens == 0 then
        return { type = "chunk", debug = false, val = {} }
    end

    tokens = clone_tokens(tokens)

    local set_debug = tokens[1] ~= nil and tokens[1].type == "debug"
    if set_debug then
        lyra.core.state.vars.debug = true
        table.remove(tokens, 1)
    end

    local lines = {}
    if #tokens > 0 then
        local statement = parse_statement(tokens)
        validate_statement(statement)
        table.insert(lines, statement)
    end

    return { type = "chunk", debug = set_debug, val = lines }
end

---@param tree LyraFrontChunk|LyraFrontStatement
---@return nil
local function run(tree)
    if tree == nil then
        lyra.vars.error = 0
        return
    end

    if tree.type ~= "chunk" then
        tree = { type = "chunk", debug = false, val = { tree } }
    end

    if #tree.val == 0 then
        lyra.vars.error = 0
        return
    end

    if tree.debug then
        lyra.vars.debug = true
    end

    for _, statement in ipairs(tree.val) do
        if statement.type == "lua" then
            lyra.api.builtin.lua({ statement.val })
        else
            run_statement(statement)
        end
    end

    if tree.debug then
        lyra.vars.debug = false
    end
end

---@param tokens LyraFrontToken[]
local function print_tokens(tokens)
    for _, token in ipairs(tokens) do
        print(token.type, token.val)
    end
end

return {
    debug = {
        print_tokens = print_tokens,
    },
    parse = parse,
    run = run,
    tokenize = tokenize,
}
