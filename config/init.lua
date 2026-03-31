core = rewsh.plugin.load("app://core")
core:setup()

cat = rewsh.api.process.command('/bin/cat');
cat:add_arg('README.md')

return_code = cat:run();
print(return_code)
