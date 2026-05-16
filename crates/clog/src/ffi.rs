use std::ffi::{CStr, c_char, c_uint};

use log::{Level as LogLevel, LevelFilter as LogLevelFilter, RecordBuilder, logger};

pub use pretty_env_logger;

/// cbindgen:prefix-with-name
/// cbindgen:rename-all=SCREAMING_SNAKE_CASE
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub enum Level {
    Off = 0,
    Error = 1,
    Warn = 2,
    Info = 3,
    Debug = 4,
    Trace = 5,
}
macro_rules! generate_arms {
    ($a: ident, $b: ident $(,$x:ident => $y:ident)* ; $(,$w: ident)*) => {
impl From<$a> for $b{
    fn from(value: $a) -> $b{
        match value{
            $($a::$x => $b::$y,)*
            $($a::$w => $b::$w,)*
        }
    }
}
    };
}

generate_arms!(Level, LogLevelFilter;, Off, Error, Warn, Info, Debug, Trace);
generate_arms!(Level, LogLevel, Off => Error;, Error, Warn, Info, Debug, Trace);

generate_arms!(LogLevelFilter, Level;, Off, Error, Warn, Info, Debug, Trace);
generate_arms!(LogLevel, Level;, Error, Warn, Info, Debug, Trace);

#[unsafe(no_mangle)]
pub extern "C" fn init_logger() {
    pretty_env_logger::init();
}

#[unsafe(no_mangle)]
pub extern "C" fn set_log(level: Level) {
    log::set_max_level(level.into());
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rust_log(line: c_uint, file: *mut c_char, level: Level, msg: *mut c_char) {
    let logger = logger();

    let msg = unsafe { CStr::from_ptr(msg) }.to_str().unwrap_or("???");
    let file = unsafe { CStr::from_ptr(file) }.to_str().ok();
    let args = format_args!("{}", msg);
    let r = RecordBuilder::new()
        .line(Some(line))
        .file(file)
        .level(level.into())
        .args(args)
        .build();

    logger.log(&r);
}
