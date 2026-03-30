pub mod ffi;
pub mod version;

use std::{
    fs::File,
    io::Read,
    path::{Path, PathBuf},
};

use anyhow::{Result, bail};
use serde::{Deserialize, Serialize};

use crate::plugin::version::Version;

/// cbindgen:prefix-with-name
/// cbindgen:rename-all=SCREAMING_SNAKE_CASE
#[repr(C)]
#[derive(Debug, Default, Serialize, Deserialize, Clone)]
pub enum PluginKind {
    Lua,
    #[default]
    C,
}

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct ManifestHeader {
    pub name: String,
    pub version: Version,
    pub edition: Version,

    #[serde(default)]
    pub kind: PluginKind,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub license: Option<Version>,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub author: Option<String>,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub src: Option<String>,
}

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct Dependency {
    pub name: String,
    pub version: Version,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub source: Option<url::Url>,
}

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct Manifest {
    pub plugin: ManifestHeader,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub dependecies: Vec<Dependency>,
}

pub struct Plugin {
    pub manifest: Manifest,
    pub location: PathBuf,
}

impl Manifest {
    pub fn load_app_plugin<P: AsRef<Path>>(path: P) -> Result<Plugin> {
        let path = path.as_ref();
        // TODO: add toggle
        let mut plugin_path = PathBuf::from("./plugins");

        plugin_path.push(path);
        let mut manifest_path = plugin_path.clone();
        manifest_path.push("rewsh");
        manifest_path.set_extension("toml");

        let mut buf = String::new();
        File::open(&manifest_path)?.read_to_string(&mut buf)?;

        return Ok(Plugin {
            manifest: toml::from_str(buf.as_str())?,
            location: plugin_path,
        });
    }

    pub fn get_manifest(source: url::Url) -> Result<Plugin> {
        let scheme = source.scheme();
        match scheme {
            "app" => Self::load_app_plugin(source.host_str().unwrap()),
            "http" | "https" => todo!("http requests not available yet"),
            _ => bail!("unsupported source location: {source}"),
        }
    }
}

#[cfg(test)]
mod test {
    use crate::plugin::Manifest;
    use anyhow::*;
    use url::Url;

    #[test]
    fn manifest_load() -> Result<()> {
        let url = Url::parse("app://core/test")?;
        Manifest::get_manifest(url)?;
        return Ok(());
    }
}
