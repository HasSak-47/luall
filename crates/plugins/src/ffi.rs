use std::{
    ffi::{CStr, CString, c_char, c_int},
    ptr,
};

use crate::{PluginData, PluginHandler, PluginKind, PluginManager, raw_bindings as rb};

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn plugin_get_name(plugin: *const PluginData) -> *mut c_char {
    let plugin = unsafe { &*plugin };

    let s = CString::new(plugin.manifest.header.name.as_str()).unwrap();

    return s.into_raw();
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn plugin_get_compilation_path(plugin: *const PluginData) -> *mut c_char {
    let plugin = unsafe { &*plugin };
    let mut path = plugin.artifact_path.clone();
    path.push("compilation");

    let s = CString::new(path.to_str().unwrap()).unwrap();

    return s.into_raw();
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn plugin_get_shared_object_path(plugin: *const PluginData) -> *mut c_char {
    let plugin = unsafe { &*plugin };
    let mut path = plugin.artifact_path.clone();
    path.push(&plugin.manifest.header.name);
    path.set_extension("so");

    let s = CString::new(path.to_str().unwrap()).unwrap();

    return s.into_raw();
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn plugin_get_data_path(plugin: *const PluginData) -> *mut c_char {
    let plugin = unsafe { &*plugin };

    let s = CString::new(plugin.root_path.to_str().unwrap()).unwrap();

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
pub extern "C" fn manager_resolve_plugin(
    manager_ptr: *mut PluginManager,
    path: *const c_char,
) -> *mut PluginData {
    let manager = unsafe { &mut (*manager_ptr) };
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = url::Url::parse(cstring.to_str().unwrap()).unwrap();

    let plugin = manager.resolve(&path);
    if plugin.is_err() {
        return ptr::null_mut();
    }

    return (plugin.unwrap()) as *mut PluginData;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn manager_unload_plugin(
    lua: *mut super::bindings::lua_State,
    manager_ptr: *mut PluginManager,
    path: *const c_char,
) -> i32 {
    let manager = unsafe { &mut (*manager_ptr) };
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = url::Url::parse(cstring.to_str().unwrap()).unwrap();

    if manager.destroy_plugin(&path, lua).is_err() {
        return -1;
    }

    return 0;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn manager_prepare_plugin(manager_ptr: *mut PluginManager, path: *const c_char) -> i32 {
    let manager = unsafe { &mut (*manager_ptr) };
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = url::Url::parse(cstring.to_str().unwrap()).unwrap();

    if manager.prepare_plugin(&path).is_err() {
        return -1;
    }

    return 0;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn get_plugin(
    manager_ptr: *mut PluginManager,
    path: *const c_char,
) -> *mut PluginData {
    let manager = unsafe { &mut (*manager_ptr) };
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = url::Url::parse(cstring.to_str().unwrap()).unwrap();

    let plugin = manager.get_plugin(&path);
    if plugin.is_none() {
        return ptr::null_mut();
    }

    return (plugin.unwrap()) as *mut PluginData;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn manager_is_plugin_prepared(
    manager_ptr: *mut PluginManager,
    path: *const c_char,
) -> c_char {
    let manager = unsafe { &mut (*manager_ptr) };
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = url::Url::parse(cstring.to_str().unwrap()).unwrap();

    return manager.is_plugin_prepared(&path) as c_char;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn plugin_get_kind(plugin: *mut PluginData) -> PluginKind {
    return unsafe { &*plugin }.manifest.header.kind.clone();
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn plugin_get_url(plugin: *mut PluginData) -> *mut c_char {
    let cstr = CString::new(unsafe { &*plugin }.source.as_str()).unwrap();
    return cstr.into_raw();
}

#[allow(unused)]
#[repr(transparent)]
#[derive(Clone, Copy)]
pub struct CIterator<T> {
    i: *mut dyn std::iter::Iterator<Item = T>,
}

fn _next_item<T>(iter: *const CIterator<*const T>) -> *const T {
    let iterator = unsafe { &mut *(*iter).i };

    if let Some(n) = iterator.next() {
        return n;
    } else {
        return std::ptr::null();
    }
}

pub type CIteratorString = CIterator<*const c_char>;

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn get_plugin_handler(plugin: *mut PluginData) -> *mut PluginHandler {
    let p = unsafe { &mut *plugin };

    if let Some(h) = &mut p.handler.handler {
        return (h as *mut rb::PluginHandler).cast();
    }
    return {
        return std::ptr::null_mut();
    };
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn next_plugin_name(iter: *const CIteratorString) -> *const c_char {
    return _next_item(iter);
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn get_plugin_dependecies_iterator(
    manager: *mut PluginManager,
    path: *const c_char,
) -> *const CIteratorString {
    let cstring = unsafe { CStr::from_ptr(path) };
    let path = url::Url::parse(cstring.to_str().unwrap()).unwrap();

    let plugin = unsafe { (&*manager) }.plugins.get(&path).unwrap();
    let plugins = plugin
        .manifest
        .dependecies
        .iter()
        .map(|depend| depend.source.as_ref().unwrap())
        .map(url::Url::as_str)
        .map(CString::new)
        .map(Result::unwrap)
        .map(CString::into_raw);

    return Box::into_raw(Box::new(CIterator {
        i: Box::into_raw(Box::new(plugins)) as *mut _,
    })) as *const _;
}

#[allow(unused)]
#[unsafe(no_mangle)]
pub extern "C" fn get_plugin_iterator(manager: *mut PluginManager) -> *const CIteratorString {
    let plugins = unsafe { (&*manager).get_prepared_plugins() }
        .into_iter()
        .map(url::Url::as_str)
        .map(CString::new)
        .map(Result::unwrap)
        .map(CString::into_raw);

    return Box::into_raw(Box::new(CIterator {
        i: Box::into_raw(Box::new(plugins)) as *mut _,
    })) as *const _;
}

#[allow(unused)]
#[unsafe(no_mangle)]

pub extern "C" fn manager_is_plugin_resolved(
    manager: *mut PluginManager,
    path: *const c_char,
) -> c_int {
    let url = url::Url::parse(unsafe { CStr::from_ptr(path) }.to_str().unwrap()).unwrap();
    let manager = unsafe { (&*manager) };

    return manager.plugins.contains_key(&url) as i32;
}
