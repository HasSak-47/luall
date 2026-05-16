use std::ffi::{CStr, c_char, c_uint};

use log::{Level as LogLevel, RecordBuilder, logger};

pub use pretty_env_logger;

/// cbindgen:prefix-with-name
/// cbindgen:rename-all=SCREAMING_SNAKE_CASE
#[repr(C)]
pub enum Level {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
    Trace = 4,
}

impl From<Level> for LogLevel {
    fn from(value: Level) -> LogLevel {
        match value {
            Level::Error => LogLevel::Error,
            Level::Warn => LogLevel::Warn,
            Level::Info => LogLevel::Info,
            Level::Debug => LogLevel::Debug,
            Level::Trace => LogLevel::Trace,
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn init_logger() {
    pretty_env_logger::init();
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
