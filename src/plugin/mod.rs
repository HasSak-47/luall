pub mod ffi;
pub mod version;

use std::{
    collections::{HashMap, HashSet},
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
    BINARY,
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
    pub version: Option<Version>,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub source: Option<url::Url>,
}

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct Manifest {
    pub plugin: ManifestHeader,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub dependecies: Vec<Dependency>,
}

/// cbindgen:prefix-with-name
/// cbindgen:rename-all=SCREAMING_SNAKE_CASE
#[derive(Debug, Default)]
pub enum LocationKind {
    CORE,
    PATH,
    #[default]
    GIT,
}

#[derive(Debug)]
pub struct Plugin {
    pub manifest: Manifest,
    pub kind: LocationKind,
    pub location: url::Url,
    // core plugins are at PLUGIN_PATH/*
    // path plugins are at the specified path
    // git plugins are written into: ./local/share/rewsh/plugins/{name}
    pub data_location: PathBuf,
}

impl Plugin {
    pub fn load_path_plugin<P: AsRef<Path>>(path: P, location: url::Url) -> Result<Plugin> {
        let path = path.as_ref();
        let mut plugin_path = path.to_path_buf();

        plugin_path.push(path);
        let mut manifest_path = plugin_path.clone();
        manifest_path.push("rewsh");
        manifest_path.set_extension("toml");

        let mut buf = String::new();
        File::open(&manifest_path)?.read_to_string(&mut buf)?;

        return Ok(Plugin {
            location,
            manifest: toml::from_str(buf.as_str())?,
            kind: LocationKind::PATH,
            data_location: plugin_path,
        });
    }

    pub fn load_core_plugin<P: AsRef<Path>>(path: P, location: url::Url) -> Result<Plugin> {
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
            location,
            manifest: toml::from_str(buf.as_str())?,
            kind: LocationKind::CORE,
            data_location: plugin_path,
        });
    }

    pub fn get_plugin(source: &url::Url) -> Result<Plugin> {
        let scheme = source.scheme();
        match scheme {
            "core" => Self::load_core_plugin(source.host_str().unwrap(), source.clone()),
            "path" => Self::load_path_plugin(source.host_str().unwrap(), source.clone()),
            "http" | "https" => todo!("http requests not available yet"),
            _ => bail!("unsupported source location: {source}"),
        }
    }
}

#[derive(Debug, Default)]
pub struct PluginManager {
    plugins: HashMap<url::Url, Plugin>,
    loaded_plugins: HashSet<url::Url>,
}

impl PluginManager {
    /*
     * get or add plugin to the manager
     */
    pub fn add_plugin(&mut self, path: &url::Url) -> Result<&mut Plugin> {
        if self.plugins.contains_key(path) {
            return Ok(self.plugins.get_mut(path).unwrap());
        }

        let path = path.clone();
        let manifest = Plugin::get_plugin(&path)?;

        for dependecy in &manifest.manifest.dependecies {
            if let Some(source) = &dependecy.source {
                self.add_plugin(source)?;
            }
        }

        self.plugins.insert(path.clone(), manifest);

        return Ok(self.plugins.get_mut(&path).unwrap());
    }

    pub fn is_plugin_loaded(&mut self, name: &url::Url) -> bool {
        return self.loaded_plugins.contains(name);
    }

    pub fn mark_plugin_as_loaded(&mut self, name: &url::Url) -> bool {
        if self.plugins.contains_key(name) {
            self.loaded_plugins.insert(name.clone());
            return true;
        }
        return false;
    }

    pub fn get_plugin(&mut self, name: &url::Url) -> Option<&mut Plugin> {
        return self.plugins.get_mut(name);
    }
}

#[cfg(test)]
mod test {
    use crate::plugin::Plugin;
    use anyhow::*;
    use url::Url;

    #[test]
    fn manifest_load() -> Result<()> {
        let url = Url::parse("core://core_front")?;
        Plugin::get_plugin(&url)?;
        return Ok(());
    }
}
