local core = rewsh.plugin.load("app://core")
core:setup()

rewsh.api.set_raw_mode()
rewsh.api.on_event('exit', rewsh.api.unset_raw_mode)

rewsh.api.on_event('key_input', function(key)
    print('key: ', string.format('%x', key))
    if key == 113 then
        rewsh.state.running = false
    end
end)
