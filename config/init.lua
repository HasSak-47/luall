local api = rewsh.plugin.require("core://api")

local buffer = {
    write = function(self, data)
        local start = self.data:sub(1, self.index)
        local finish = self.data:sub(self.index + 1)
        self.data = start .. data .. finish
    end,
}

buffer.__index = buffer
buffer.new = function()
    local b = {
        index = 0,
        data = '',
    }

    setmetatable(b, buffer)
    return b
end

local view = {
    write = function(self, data)
        local start = self.data:sub(1, self.index)
        local finish = self.data:sub(self.index + 1)
        self.data = start .. data .. finish
    end,
}

view.__index = view
view.new = function(x, y)
    if x == nil then
        x = 1
    end
    if y == nil then
        y = 1
    end


    local v = {
        x = x,
        y = y,

        buffer = buffer.new()
    }

    setmetatable(v, view)
    return v
end


local input_state = {
    data = '',
    index = 0
}
local function submit_input()
    local line = input_state.data

    io.stdout:write("\r" .. rewsh.api.prompt() .. line .. "\n")

    local ok, err = pcall(rewsh.api.parser.parse, line)
    if not ok then
        print('failed to parse', err)
        return
    end

    input_state.data = ""
    input_state.index = 0
end


---@param input RewshInputKey
---@return nil
local function handle_input(input)
    if input.kind == "letter" then
        if input.letter == "c" and input.ctrl then
            rewsh.state.running = false
            return
        end

        local start = input_state.data:sub(1, input_state.index)
        local finish = input_state.data:sub(input_state.index + 1)
        input_state.data = start .. input.letter .. finish
        input_state.index = input_state.index + 1
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
            end
        elseif input.special == "enter" then
            submit_input()
        end
    end

    rewsh.api.render_input(input_state.data, input_state.index)
end

rewsh.api.on_event("enter", rewsh.api.set_raw_mode)
rewsh.api.on_event("exit", rewsh.api.unset_raw_mode)
rewsh.api.on_event("enter", function()
    rewsh.api.render_input(input_state.data, input_state.index)
end)
rewsh.api.on_event("key_input", handle_input)
