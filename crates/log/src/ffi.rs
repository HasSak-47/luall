use std::{
    ffi::{CStr, c_char, c_int, c_uint},
    fmt::Display,
    os::fd::{AsFd, AsRawFd, FromRawFd},
};

use log::{Level as LogLevel, LevelFilter as LogLevelFilter, RecordBuilder, logger};

use env_logger::{
    Target,
    fmt::style::{AnsiColor, Style},
};

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

impl Display for Level {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", log::LevelFilter::from(*self))
    }
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
pub extern "C" fn init_logger(fd: c_int) {
    let writter = unsafe { std::fs::File::from_raw_fd(fd) };
    env_logger::builder()
        .target(Target::Pipe(Box::new(writter)))
        .filter_level(LogLevelFilter::Trace)
        .write_style(env_logger::WriteStyle::Always)
        .format(|f, record| {
            use std::io::Write;
            let target = record.target();
            let max_target_width = crate::pretty::max_target_width(target);
            let level = record.level();
            let line = record.line().unwrap_or(0);

            let level_style = Style::new().fg_color(Some(
                match level {
                    LogLevel::Trace => AnsiColor::Magenta,
                    LogLevel::Debug => AnsiColor::Blue,
                    LogLevel::Info => AnsiColor::Green,
                    LogLevel::Warn => AnsiColor::Yellow,
                    LogLevel::Error => AnsiColor::Red,
                }
                .into(),
            ));
            let target_style = Style::new().bold();

            writeln!(
                f,
                "{level_style}{level}{level_style:#} {target_style}{target:>max_target_width$}{target_style:#}:{line} > {}",
                record.args(),
            )
        })
        .init();
}

#[unsafe(no_mangle)]
pub extern "C" fn set_log_level(level: Level) {
    log::set_max_level(level.into());
}

#[unsafe(no_mangle)]
pub extern "C" fn get_log_level() -> Level {
    return log::max_level().into();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rust_log(
    level: Level,
    line: c_uint,
    file: *const c_char,
    target: *const c_char,
    msg: *const c_char,
) {
    if log::LevelFilter::from(level) > log::max_level() {
        return;
    }
    let logger = logger();

    let msg = unsafe { CStr::from_ptr(msg) }.to_str().unwrap_or("???");
    let file = if file.is_null() {
        None
    } else {
        unsafe { CStr::from_ptr(file) }.to_str().ok()
    };

    let target = if target.is_null() {
        "??".to_string()
    } else {
        unsafe { CStr::from_ptr(target) }
            .to_str()
            .unwrap_or("")
            .to_string()
    };
    let args = format_args!("{}", msg);
    let r = RecordBuilder::new()
        .level(level.into())
        .line(if line != 0 { Some(line) } else { None })
        .target(&target)
        .file(file)
        .target(file.unwrap_or(""))
        .module_path(file)
        .args(args)
        .build();

    logger.log(&r);
    logger.flush();
}
