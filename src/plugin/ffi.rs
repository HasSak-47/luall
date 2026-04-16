use std::{
    ffi::{CStr, CString, c_char},
    ptr,
};

use crate::plugin::{Plugin, PluginKind, PluginManager};

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

    let s = CString::new(plugin.data_location.to_str().unwrap()).unwrap();

    return s.into_raw();
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn delete_plugin_manager(ptr: *mut PluginManager) {
    Box::from(ptr);
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn new_plugin_manager() -> *mut PluginManager {
    return Box::leak(Box::new(PluginManager::default()));
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn add_or_get_plugin(
    manager_ptr: *mut PluginManager,
    path: *const c_char,
) -> *const Plugin {
    let manager = unsafe { &mut (*manager_ptr) };
    let cstring = unsafe { CStr::from_ptr(path) };

    let path = cstring.to_str().unwrap();

    let plugin = manager.add_plugin(&url::Url::parse(&path.to_string()).unwrap());
    if plugin.is_err() {
        return ptr::null();
    }

    return (plugin.unwrap()) as *mut Plugin;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn add_plugin(manager_ptr: *mut PluginManager, path: *const c_char) -> *mut Plugin {
    let manager = unsafe { &mut (*manager_ptr) };
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = url::Url::parse(cstring.to_str().unwrap()).unwrap();

    let plugin = manager.add_plugin(&path);
    if plugin.is_err() {
        return ptr::null_mut();
    }

    return (plugin.unwrap()) as *mut Plugin;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn get_plugin(manager_ptr: *mut PluginManager, path: *const c_char) -> *mut Plugin {
    let manager = unsafe { &mut (*manager_ptr) };
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = url::Url::parse(cstring.to_str().unwrap()).unwrap();

    let plugin = manager.get_plugin(&path);
    if plugin.is_none() {
        return ptr::null_mut();
    }

    return (plugin.unwrap()) as *mut Plugin;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn is_plugin_loaded(manager_ptr: *mut PluginManager, path: *const c_char) -> c_char {
    let manager = unsafe { &mut (*manager_ptr) };
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = url::Url::parse(cstring.to_str().unwrap()).unwrap();

    return manager.is_plugin_loaded(&path) as c_char;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn mark_plugin_as_loaded(
    manager_ptr: *mut PluginManager,
    path: *const c_char,
) -> c_char {
    let manager = unsafe { &mut (*manager_ptr) };
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = url::Url::parse(cstring.to_str().unwrap()).unwrap();

    return manager.mark_plugin_as_loaded(&path) as c_char;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn plugin_get_kind(plugin: *mut Plugin) -> PluginKind {
    return unsafe { &*plugin }.manifest.plugin.kind.clone();
}
