local core = rewsh.plugin.load("app://core")
core:setup()

local cat_path = rewsh.api.path.parse('/bin')
cat_path:push('cat')

local cat = rewsh.api.process.command(cat_path:to_string());
cat:add_arg('README.md')

local return_value = cat:run()
rewsh.state.running = false
