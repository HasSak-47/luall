use super::raw_bindings as rb;
use crate::plugin::{PluginData, PluginKind};
use anyhow::Result;

/// cbindgen:ignore
pub(crate) use rb::lua_State;

#[derive(Debug, Default)]
pub struct PluginHandlerWrapper {
    pub handler: Option<rb::PluginHandler>,
}

impl PluginHandlerWrapper {
    pub fn is_loaded(&self) -> bool {
        return self.handler.is_some();
    }

    pub fn load_plugin(&mut self, data: &PluginData) -> Result<()> {
        self.handler = Some(unsafe {
            match data.manifest.plugin.kind {
                PluginKind::Lua => rb::load_lua_plugin((data as *const PluginData).cast()),
                PluginKind::C => rb::load_c_plugin((data as *const PluginData).cast()),
                PluginKind::BINARY => rb::load_binary_plugin((data as *const PluginData).cast()),
            }
        });

        return Ok(());
    }

    pub fn unload_plugin(&mut self, lua: *mut rb::lua_State) -> Result<()> {
        if let Some(handler) = &mut self.handler {
            unsafe {
                match handler.kind {
                    PluginKind::Lua => {
                        rb::unload_lua_plugin(lua, handler);
                        self.handler = None;
                    }

                    PluginKind::C => {
                        rb::unload_c_plugin(lua, handler);
                        self.handler = None;
                    }

                    PluginKind::BINARY => {
                        rb::unload_binary_plugin(lua, handler);
                        self.handler = None;
                    }
                }
            }
        }

        return Ok(());
    }
}
