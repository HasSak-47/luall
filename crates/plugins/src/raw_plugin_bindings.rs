#![allow(warnings)]
use super::{PluginData, PluginKind};

include!(concat!(
    env!("OUT_DIR"),
    "/________include_plugin_bindings.h_raw_bindings.rs"
));
