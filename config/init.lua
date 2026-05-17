lyra.plugin.require("core://runtime")
lyra.plugin.require("core://api")

local input_state = {
    history = {
        index = 0,
        history = {},
        current = "",
    },
    data = '',
    index = 0
}

local function submit_input()
    local line = input_state.data

    io.stdout:write("\r" .. lyra.api.prompt() .. line .. "\n")

    if line ~= "" then
        local ok, err = pcall(lyra.api.parser.parse, line)
        if not ok then
            print('failed to parse', err)
            return
        end
    else
        return
    end

    if input_state.history.history[1] ~= line then
        table.insert(input_state.history.history, 1, line)
    end
    input_state.data = ""
    input_state.index = 0
    input_state.history.index = 0
    input_state.history.current = ""
end


---@param input LyraInputKey
---@return nil
local function handle_input(input)
    if input.kind == "letter" then
        if input.ctrl then
            if input.letter == "c" then
                lyra.core.state.is_running = false
            end
            return
        end

        local start = input_state.data:sub(1, input_state.index)
        local finish = input_state.data:sub(input_state.index + 1)
        input_state.data = start .. input.letter .. finish
        input_state.index = input_state.index + 1
        input_state.history.index = 0
        input_state.history.current = input_state.data
    elseif input.kind == "special" then
        if input.special == "left" then
            if input_state.index > 0 then
                input_state.index = input_state.index - 1
            end
        elseif input.special == "right" then
            if input_state.index < input_state.data:len() then
                input_state.index = input_state.index + 1
            end
        elseif input.special == "backspace" then
            if input_state.index > 0 then
                local start = input_state.data:sub(1, input_state.index - 1)
                local finish = input_state.data:sub(input_state.index + 1)
                input_state.data = start .. finish
                input_state.index = input_state.index - 1
                input_state.history.index = 0
                input_state.history.current = input_state.data
            end
        elseif input.special == "enter" then
            submit_input()
        elseif input.special == "up" then
            local history = input_state.history
            if history.index == 0 then
                history.current = input_state.data
            end

            local available = #history.history
            if available == 0 then
                lyra.api.render_input(input_state.data, input_state.index)
                return
            end

            local next = history.index + 1
            if next > available then
                next = available
            end

            history.index = next
            input_state.data = history.history[next] or ""
            input_state.index = #input_state.data
        elseif input.special == "down" then
            local history = input_state.history
            if history.index == 0 then
                lyra.api.render_input(input_state.data, input_state.index)
                return
            end

            local next = history.index - 1
            history.index = next

            if next == 0 then
                input_state.data = history.current
            else
                input_state.data = history.history[next] or ""
            end
            input_state.index = #input_state.data
        end
    end

    lyra.api.render_input(input_state.data, input_state.index)
end

lyra.api.on_event("enter", lyra.api.set_raw_mode)
lyra.api.on_event("exit", lyra.api.unset_raw_mode)
lyra.api.on_event("enter", function()
    lyra.api.render_input(input_state.data, input_state.index)
end)
lyra.api.on_event("key_input", handle_input)
