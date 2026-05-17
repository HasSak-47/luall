pub mod ffi;

use clap::Parser;
use log::LevelFilter;

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
pub struct Args {
    #[arg(short, long, default_value_t = LevelFilter::Off)]
    log_level: LevelFilter,
    script: Option<String>,
}
