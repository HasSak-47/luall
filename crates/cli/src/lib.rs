pub mod ffi;

use clap::Parser;

#[derive(Parser, Debug, Default)]
#[command(version, about, long_about = None)]
pub struct Args {
    #[arg(short, long, default_value_t = false)]
    debug: bool,
    script: Option<String>,
}
