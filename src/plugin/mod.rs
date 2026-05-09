pub mod bindings;
pub mod ffi;
pub mod version;

/// cbindgen:ignore
mod raw_bindings;

use std::{
    collections::{HashMap, HashSet},
    fs::File,
    io::Read,
    path::{Path, PathBuf},
};

use anyhow::{Result, bail};
use serde::{Deserialize, Serialize};

use crate::plugin::{bindings::PluginHandlerWrapper, version::Version};

pub struct PluginHandler {
    _opaque: [u8; 0],
}

/// cbindgen:prefix-with-name
/// cbindgen:rename-all=SCREAMING_SNAKE_CASE
#[repr(C)]
#[derive(Debug, Default, Serialize, Deserialize, Clone, Copy)]
pub enum PluginKind {
    Lua = 0,
    BINARY = 1,
    #[default]
    C = 2,
    Rust = 3,
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

fn default_true() -> bool {
    true
}

fn is_true(v: &bool) -> bool {
    *v
}

#[derive(Debug, Default, Clone, Serialize, Deserialize)]
pub struct Dependency {
    pub name: String,
    pub version: Option<Version>,

    #[serde(default = "default_true", skip_serializing_if = "is_true")]
    pub optional: bool,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub source: Option<url::Url>,
}

#[derive(Debug, Serialize, Deserialize)]
pub enum KindOptions {
    C { libraries: Vec<String> },
}

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct Manifest {
    pub plugin: ManifestHeader,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub opts: Option<KindOptions>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub dependecies: Vec<Dependency>,
}

impl Manifest {
    fn generate_manifest<S: AsRef<str>>(src: S) -> Result<Manifest> {
        let manifest: Manifest = toml::from_str(src.as_ref())?;

        if let Some(opts) = &manifest.opts {
            match (opts, manifest.plugin.kind) {
                (KindOptions::C { .. }, PluginKind::C) => {}
                _ => {
                    bail!(
                        "manifest is invalid!, opts passed are not of the same kind as the header"
                    )
                }
            }
        }

        return Ok(manifest);
    }
}

/// cbindgen:prefix-with-name
/// cbindgen:rename-all=SCREAMING_SNAKE_CASE
#[derive(Debug, Default)]
pub enum SourceKind {
    CORE,
    PATH,
    #[default]
    GIT,
}

#[derive(Debug)]
pub struct PluginData {
    pub manifest: Manifest,
    pub kind: SourceKind,
    pub source_url: url::Url,
    // core plugins are at CORE_PLUGIN_PATH/*
    // path plugins are at the specified path
    // git plugins are stored into: $STATE_PATH/rewsh/plugins/{name}

    // binary cache is at $STATE_PATH/rewsh/cache/{name}.so
    pub root_path: PathBuf,
    pub artifact_path: PathBuf,
    pub handler: PluginHandlerWrapper,
}

impl PluginData {
    pub fn resolve_from_path<P: AsRef<Path>>(path: P, location: url::Url) -> Result<PluginData> {
        let path = path.as_ref();
        let mut plugin_path = path.to_path_buf();

        plugin_path.push(path);
        let mut manifest_path = plugin_path.clone();
        manifest_path.push("rewsh");
        manifest_path.set_extension("toml");

        let mut buf = String::new();
        File::open(&manifest_path)?.read_to_string(&mut buf)?;
        let manifest: Manifest = toml::from_str(buf.as_str())?;

        let cache_path = PathBuf::from(&format!("./.ignore/cache/{}", manifest.plugin.name));

        return Ok(PluginData {
            source_url: location,
            manifest,
            kind: SourceKind::PATH,
            root_path: plugin_path,
            artifact_path: cache_path,
            handler: PluginHandlerWrapper::default(),
        });
    }

    pub fn resolves_from_core<P: AsRef<Path>>(path: P, location: url::Url) -> Result<PluginData> {
        let path = path.as_ref();
        // TODO: add toggle to set to a normal xdg path
        let mut plugin_path = std::path::absolute(PathBuf::from("./plugins"))?;

        plugin_path.push(path);
        let mut manifest_path = plugin_path.clone();
        manifest_path.push("rewsh");
        manifest_path.set_extension("toml");

        let mut buf = String::new();
        File::open(&manifest_path)?.read_to_string(&mut buf)?;
        let manifest: Manifest = toml::from_str(buf.as_str())?;

        let cache_path = std::path::absolute(PathBuf::from(&format!(
            "./.ignore/cache/{}/",
            manifest.plugin.name
        )))?;

        return Ok(PluginData {
            source_url: location,
            manifest,
            kind: SourceKind::CORE,
            root_path: plugin_path,
            artifact_path: cache_path,
            handler: PluginHandlerWrapper::default(),
        });
    }

    pub fn resolve(source: &url::Url) -> Result<PluginData> {
        let scheme = source.scheme();
        match scheme {
            "core" => Self::resolves_from_core(source.host_str().unwrap(), source.clone()),
            "path" => Self::resolve_from_path(source.host_str().unwrap(), source.clone()),
            "http" | "https" => todo!("http requests not available yet"),
            _ => bail!("unsupported source location: {source}"),
        }
    }
}

#[derive(Debug, Default)]
pub struct PluginManager {
    plugins: HashMap<url::Url, PluginData>,
    loaded_plugins: HashSet<url::Url>,
}

impl PluginManager {
    /*
     * 'load' makes finding a plugin and loading it's manifest to the manager
     */
    pub fn resolve(&mut self, path: &url::Url) -> Result<&mut PluginData> {
        if self.plugins.contains_key(path) {
            return Ok(self.plugins.get_mut(path).unwrap());
        }
        let manifest = PluginData::resolve(path)?;

        for dependecy in &manifest.manifest.dependecies {
            if !dependecy.optional && self.plugins.contains_key(path) {
                if let Some(source) = &dependecy.source {
                    self.resolve(source)?;
                }
            }
        }

        self.plugins.insert(path.clone(), manifest);

        return Ok(self.plugins.get_mut(&path).unwrap());
    }

    pub fn is_plugin_loaded(&mut self, name: &url::Url) -> bool {
        return self.loaded_plugins.contains(name);
    }

    pub fn get_plugin(&mut self, name: &url::Url) -> Option<&mut PluginData> {
        return self.plugins.get_mut(name);
    }

    // TODO: not use raw bindings just make bindings export a lua_state
    /*
     * 'unload' loads a plugin into the memory but does not run it
     */
    pub fn destroy_plugin(
        &mut self,
        name: &url::Url,
        lua: *mut raw_bindings::lua_State,
    ) -> Result<()> {
        if !self.plugins.contains_key(name) {
            self.resolve(&name)?;
        }

        let data = &mut self.plugins.get_mut(name).unwrap();
        let mut handler = std::mem::take(&mut data.handler);
        handler.unload_plugin(lua)?;
        data.handler = handler;
        return Ok(());
    }

    /*
     * 'load' loads a plugin into the memory but does not run it
     */
    pub fn load_plugin(&mut self, name: &url::Url) -> Result<()> {
        if !self.plugins.contains_key(name) {
            self.resolve(&name)?;
        }

        for dependency in self.plugins[name].manifest.dependecies.clone() {
            // load plugin only if it is resolved or is not optional
            if !dependency.optional && self.plugins.contains_key(name) {
                if let Some(source) = &dependency.source {
                    self.load_plugin(source)?;
                }
            }
        }

        let data = &mut self.plugins.get_mut(name).unwrap();

        let mut handler = std::mem::take(&mut data.handler);
        handler.load_plugin(data)?;
        data.handler = handler;
        return Ok(());
    }

    pub fn get_loaded_plugins(&self) -> &HashSet<url::Url> {
        return &self.loaded_plugins;
    }
}

#[cfg(test)]
mod test {
    use crate::plugin::PluginData;
    use anyhow::*;
    use url::Url;

    #[test]
    fn manifest_load() -> Result<()> {
        let url = Url::parse("core://runtime")?;
        PluginData::resolve(&url)?;
        return Ok(());
    }
}
