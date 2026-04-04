local core = rewsh.plugin.load("app://core")
core:setup()

local cat = rewsh.api.process.command('/bin/cat')
cat:add_arg('README.md')
local grep = rewsh.api.process.command('/bin/grep')
grep:add_arg('git')

local pipe = rewsh.api.process.pipe()

cat:bind_pipe(pipe, "write")
grep:bind_pipe(pipe, "read")

cat:run()
grep:run()

pipe:close()

cat:wait()
grep:wait()

rewsh.state.running = false
