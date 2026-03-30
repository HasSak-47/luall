use std::ffi::{CStr, CString, c_char};

use crate::plugin::{Manifest, Plugin, PluginKind};

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn plugin_get_name(plugin: *mut Plugin) -> *mut c_char {
    let plugin = unsafe { &*plugin };

    let s = CString::new(plugin.manifest.plugin.name.as_str()).unwrap();

    return s.into_raw();
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn plugin_get_path(plugin: *mut Plugin) -> *mut c_char {
    let plugin = unsafe { &*plugin };

    let s = CString::new(plugin.location.to_str().unwrap()).unwrap();

    return s.into_raw();
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn get_plugin(path: *const c_char) -> *mut Plugin {
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = cstring.to_str().unwrap();

    let plugin = Manifest::get_manifest(url::Url::parse(path).unwrap()).unwrap();

    return Box::leak(Box::new(plugin));
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn plugin_get_kind(plugin: *mut Plugin) -> PluginKind {
    return unsafe { &*plugin }.manifest.plugin.kind.clone();
}
