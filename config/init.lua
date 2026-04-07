local core = rewsh.plugin.load("app://core_back")
core:setup()

rewsh.api.on_event('enter', rewsh.api.set_raw_mode)
rewsh.api.on_event('exit', rewsh.api.unset_raw_mode)

local home_string = rewsh.state.vars.user.home:to_string()
print(home_string)

local function _prompt()
    local path_str = rewsh.state.vars.cwd:to_string()
    path_str = path_str:gsub(home_string, '~')
    return rewsh.state.vars.user.name .. '@' .. rewsh.state.vars.host .. ' ' .. path_str .. '$'
end

local function prompt()
    local ok, p = pcall(_prompt)
    if not ok then
        return '$>'
    end
    return p
end

local input_state = {
    data = '',
    index = 0,
}

local function render_input()
    local line = '\r' .. prompt() .. input_state.data
    io.stdout:write(line)
    io.stdout:write('\x1b[K')

    local step_back = input_state.data:len() - input_state.index
    if step_back > 0 then
        io.stdout:write(string.format('\x1b[%dD', step_back))
    end

    io.stdout:flush()
end

rewsh.api.on_event('enter', function()
    render_input()
end)

local parser = require('config.parser')

---@param input RewshInputKey
local function handle_input(input)
    -- handle input logic
    if input.kind == 'letter' then
        if input.letter == 'c' and input.ctrl then
            rewsh.state.running = false
            return
        end
        local start = input_state.data:sub(1, input_state.index)
        local _end = input_state.data:sub(input_state.index + 1)
        input_state.data = start .. input.letter .. _end
        input_state.index = input_state.index + 1
    elseif input.kind == 'special' then
        if input.special == 'left' then
            if input_state.index > 0 then
                input_state.index = input_state.index - 1
            end
        elseif input.special == 'right' then
            if input_state.index < input_state.data:len() then
                input_state.index = input_state.index + 1
            end
        elseif input.special == 'backspace' then
            if input_state.index > 0 then
                local start = input_state.data:sub(1, input_state.index - 1)
                local _end = input_state.data:sub(input_state.index + 1)
                input_state.data = start .. _end
                input_state.index = input_state.index - 1
            end
        elseif input.special == 'enter' then
            -- write command one las ttime
            local o = '\r' .. prompt() .. input_state.data .. '\n'
            io.stdout:write(o)
            if input_state.data == 'reload' then
                rewsh.state.reload = true
                input_state.data = ''
                input_state.index = 0
                return
            end
            if input_state.data == 'exit' then
                rewsh.state.running = false
                input_state.data = ''
                input_state.index = 0
                return
            end

            local ok, _ = pcall(parser.parser, input_state.data)
            if not ok then
                print('failed to parse...')
            end

            input_state.data = ''
            input_state.index = 0
        end
    end

    render_input()
end

rewsh.api.on_event('key_input', handle_input)
