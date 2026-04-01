local core = rewsh.plugin.load("app://core")
core:setup()

local cat_path = rewsh.api.path.parse('/bin')
cat_path:push('cat')

local cat = rewsh.api.process.command(cat_path:to_string());
cat:add_arg('README.md')

rewsh.api.enter_alternate_screen()
cat:run()
cat:wait()

rewsh.api.leave_alternate_screen()

rewsh.state.running = false
