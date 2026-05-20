pub mod ffi;

use std::path::PathBuf;

use clap::Parser;
use dirs::{cache_dir, config_dir};
use log::LevelFilter;

fn default_cache_path() -> PathBuf {
    if cfg!(debug_assertions) {
        return PathBuf::from(".ignore/cache");
    }

    let mut path = cache_dir().unwrap_or_else(|| PathBuf::from("."));
    path.push("lyra");
    path
}

fn default_config_path() -> PathBuf {
    if cfg!(debug_assertions) {
        return PathBuf::from("./config/init.lua");
    }

    let mut path = config_dir().unwrap_or_else(|| PathBuf::from("."));
    path.push("lyra");
    path.push("init.lua");
    path
}

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
pub struct Args {
    #[arg(short, long, default_value_t = LevelFilter::Off)]
    log_level: LevelFilter,
    #[arg(short, long)]
    script: Option<PathBuf>,
    #[arg(long, default_value_os_t = default_cache_path())]
    cache_path: PathBuf,
    #[arg(long, default_value_os_t = default_config_path())]
    config_path: PathBuf,
}
