local core = rewsh.plugin.load("app://core")
core:setup()

rewsh.api.on_event('enter', rewsh.api.set_raw_mode)
rewsh.api.on_event('exit', rewsh.api.unset_raw_mode)

local function prompt()
    return '$>'
end

local input_state = {
    data = '',
    index = 0,
}

rewsh.api.on_event('enter', function()
    local o = '\r' .. prompt() .. input_state.data
    io.stdout:write(o)
    io.stdout:flush()
end)

local parser = require('config.parser')

---@param input RewshInputKey
local function handle_input(input)
    -- clear the line
    io.stdout:write('\r')
    for _ = 1, ('\r' .. prompt() .. input_state.data):len(), 1 do
        io.stdout:write(' ')
    end

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

            local ok, _ = pcall(parser.parser, input_state.data)
            if not ok then
                print('failed to parse...')
            end

            input_state.data = ''
            input_state.index = 0
        end
    end

    -- rewrite the line
    local o = '\r' .. prompt() .. input_state.data
    io.stdout:write(o)
    -- move the cursor to the current index
    local step_back = (input_state.data:len() - input_state.index) + 1
    io.stdout:write(string.format('\x1b[%dD', step_back))


    io.stdout:flush()
end

rewsh.api.on_event('key_input', handle_input)
