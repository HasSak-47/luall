pub mod ffi;

use clap::Parser;
use log::Level;

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
pub struct Args {
    #[arg(short, long, default_value_t = Level::Warn)]
    log_level: Level,
    script: Option<String>,
}
