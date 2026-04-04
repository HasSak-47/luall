local core = rewsh.plugin.load("app://core")
core:setup()

rewsh.api.set_raw_mode()
rewsh.api.on_event('exit', rewsh.api.unset_raw_mode)

rewsh.api.on_event('key_input', function(key)
    print('key:', tostring(key))
    if key.kind == 'letter' and key.letter == 'q' then
        rewsh.state.running = false
    end
end)
