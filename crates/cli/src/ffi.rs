pub use lyra_log::ffi::Level;

use super::*;

use std::{
    ffi::{CStr, c_char, c_int},
    ptr::null,
};

#[unsafe(no_mangle)]
pub unsafe extern "C" fn args_parse(argc: c_int, argv: *const *const c_char) -> *mut Args {
    let mut args = Vec::new();
    unsafe {
        for i in 0..(argc as usize) {
            let arg = CStr::from_ptr(*(argv.add(i)));
            if let Ok(s) = arg.to_str() {
                args.push(s.to_string());
            }
        }
    }

    let args = Args::parse_from(args);
    return Box::into_raw(Box::new(args));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn args_get_level(args: *const Args) -> Level {
    if args.is_null() {
        return Level::Warn;
    }
    unsafe {
        return (&*args).log_level.into();
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn args_get_script(args: *const Args) -> *const c_char {
    if args.is_null() {
        return null();
    }
    unsafe {
        if let Some(s) = &(&*args).script {
            return s.as_ptr() as *const c_char;
        }
    }

    return null();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn args_delete(args: *mut Args) {
    if !args.is_null() {
        unsafe {
            drop(Box::from_raw(args));
        }
    }
}
