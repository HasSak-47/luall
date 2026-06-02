use super::raw_plugin_bindings as rb;
use crate::{PluginData, PluginKind};
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

    pub fn prepare_plugin(&mut self, data: &PluginData) -> Result<()> {
        macro_rules! prepare_plugin {($($kind:path => $function:path, )+) => {
            match data.manifest.header.kind {
                $($kind => $function((data as *const PluginData).cast()),)+
                #[allow(unreachable_patterns)]
                _ => unimplemented!(),
            }};
        }

        self.handler = Some(unsafe {
            prepare_plugin!(
                PluginKind::Lua => rb::prepare_lua_plugin,
                PluginKind::C => rb::prepare_c_plugin,
                PluginKind::Rust => rb::prepare_rust_plugin,
                PluginKind::BINARY => rb::prepare_binary_plugin,
            )
        });

        return Ok(());
    }

    pub fn unload_plugin(&mut self, lua: *mut rb::lua_State) -> Result<()> {
        if let Some(handler) = &mut self.handler {
            macro_rules! unload_plugin { ($($kind:path => $function:path, )+) => {
                match handler.kind {
                    $($kind => { $function(lua, handler); self.handler = None; })+
                    #[allow(unreachable_patterns)]
                    _ => unimplemented!(),
                }};
            }
            unsafe {
                unload_plugin!(
                    PluginKind::Lua => rb::unload_lua_plugin,
                    PluginKind::C => rb::unload_c_plugin,
                    PluginKind::Rust => rb::unload_rust_plugin,
                    PluginKind::BINARY => rb::unload_binary_plugin,
                )
            }
        }

        return Ok(());
    }
}
