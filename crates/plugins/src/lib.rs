pub mod bindings;
pub mod ffi;
pub mod version;

/// cbindgen:ignore
mod raw_bindings;

use std::{
    collections::{HashMap, HashSet},
    fs::File,
    io::Read,
    path::PathBuf,
    sync::LazyLock,
};

use anyhow::{Result, bail};
use serde::{Deserialize, Serialize};

use crate::{bindings::PluginHandlerWrapper, version::Version};

pub struct PluginHandler {
    _opaque: [u8; 0],
}

/// cbindgen:prefix-with-name
/// cbindgen:rename-all=SCREAMING_SNAKE_CASE
#[repr(C)]
#[derive(Debug, Default, Serialize, Deserialize, Clone, Copy, PartialEq)]
pub enum PluginKind {
    Lua = 0,
    BINARY = 1,
    #[default]
    C = 2,
    Rust = 3,
}

#[derive(Debug, Default, Serialize, Deserialize, PartialEq)]
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

fn default_false() -> bool {
    false
}

fn is_false(v: &bool) -> bool {
    *v == false
}

#[derive(Debug, Default, Clone, Serialize, Deserialize, PartialEq)]
pub struct Dependency {
    pub name: String,
    pub version: Option<Version>,

    #[serde(default = "default_false", skip_serializing_if = "is_false")]
    pub optional: bool,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub source: Option<url::Url>,
}

#[derive(Debug, Serialize, Deserialize, PartialEq)]
pub enum KindOptions {
    C { libraries: Vec<String> },
}

#[derive(Debug, Default, Serialize, Deserialize, PartialEq)]
pub struct Manifest {
    #[serde(rename = "plugin")]
    pub header: ManifestHeader,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub opts: Option<KindOptions>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub dependecies: Vec<Dependency>,
}

impl Manifest {
    fn generate_manifest<S: AsRef<str>>(src: S) -> Result<Manifest> {
        let manifest: Manifest = toml::from_str(src.as_ref())?;

        if let Some(opts) = &manifest.opts {
            match (opts, manifest.header.kind) {
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
#[derive(Debug, Default, PartialEq)]
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
    pub source: url::Url,
    // core plugins are at CORE_PLUGIN_PATH/*
    // path plugins are at the specified path
    // git plugins are stored into: $STATE_PATH/lyra/plugins/{name}
    pub root_path: PathBuf,
    // binary cache is at $STATE_PATH/lyra/plugin_artifacts/{name}
    pub artifact_path: PathBuf,
    pub handler: PluginHandlerWrapper,
}

impl PartialEq for PluginData {
    fn eq(&self, other: &Self) -> bool {
        self.manifest == other.manifest
            && self.kind == other.kind
            && self.root_path == other.root_path
            && self.artifact_path == other.artifact_path
    }
}

/// cbindgen:ignore
const STATE_PATH: LazyLock<PathBuf> = LazyLock::new(|| {
    if cfg!(debug_assertions) {
        PathBuf::from(".ignore/cache")
    } else {
        let mut state = dirs::state_dir().unwrap();
        state.push("lyra");
        state
    }
});

impl PluginData {
    fn _resolve_from_disk(
        root_path: PathBuf,
        source: &url::Url,
        kind: SourceKind,
    ) -> Result<PluginData> {
        let mut manifest_path = root_path.clone();
        manifest_path.push("lyra");
        manifest_path.set_extension("toml");

        let mut buf = String::new();
        File::open(&manifest_path)?.read_to_string(&mut buf)?;
        let manifest = Manifest::generate_manifest(buf.as_str())?;

        let mut artifact_path = STATE_PATH.clone();
        artifact_path.push("plugin_artifacts");
        artifact_path.push(&manifest.header.name);

        return Ok(PluginData {
            source: source.clone(),
            manifest,
            kind,
            root_path,
            artifact_path,
            handler: PluginHandlerWrapper::default(),
        });
    }

    pub fn resolve_from_path(source: &url::Url) -> Result<PluginData> {
        let path = Self::path_from_url(source)?;
        let plugin_path = std::path::absolute(path)?;
        return Self::_resolve_from_disk(plugin_path, source, SourceKind::PATH);
    }

    pub fn resolves_from_core(source: &url::Url) -> Result<PluginData> {
        // TODO: add toggle to set to a normal filesytem path
        let mut plugin_path = std::path::absolute(PathBuf::from("./plugins"))?;
        plugin_path.push(source.host_str().unwrap());
        log::debug!(
            "resolving core plugin: {source} @ {}",
            plugin_path.display()
        );

        return Self::_resolve_from_disk(plugin_path, source, SourceKind::CORE);
    }

    fn path_from_url(source: &url::Url) -> Result<PathBuf> {
        if let Some(host) = source.host_str() {
            println!("{host}");
            let mut path = PathBuf::from(host);
            let suffix = source.path().trim_start_matches('/');
            if !suffix.is_empty() {
                path.push(suffix);
            }

            return Ok(path);
        }

        let path = source.path();
        if path.is_empty() {
            bail!("path source is missing a filesystem path: {source}");
        }

        return Ok(PathBuf::from(path));
    }

    pub fn resolve(source: &url::Url) -> Result<PluginData> {
        let scheme = source.scheme();
        match scheme {
            "core" => Self::resolves_from_core(source),
            "path" => Self::resolve_from_path(source),
            "http" | "https" => todo!("http requests not available yet"),
            _ => bail!("unsupported source location: {source}"),
        }
    }
}

#[derive(Debug, Default)]
pub struct PluginManager {
    plugins: HashMap<url::Url, PluginData>,
    prepared_plugins: HashSet<url::Url>,
}

impl PluginManager {
    /*
     * 'resolve' finds a plugin and loads its manifest into the manager
     */
    pub fn resolve(&mut self, path: &url::Url) -> Result<&mut PluginData> {
        if self.plugins.contains_key(path) {
            return Ok(self.plugins.get_mut(path).unwrap());
        }
        log::debug!("resolving plugin {path}");
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

    pub fn is_plugin_prepared(&mut self, name: &url::Url) -> bool {
        return self.prepared_plugins.contains(name);
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
            return Ok(());
        }

        let data = &mut self.plugins.get_mut(name).unwrap();
        let mut handler = std::mem::take(&mut data.handler);
        handler.unload_plugin(lua)?;
        data.handler = handler;
        return Ok(());
    }

    /*
     * 'prepare' loads a plugin into memory but does not run it
     */
    pub fn prepare_plugin(&mut self, name: &url::Url) -> Result<()> {
        if !self.plugins.contains_key(name) {
            self.resolve(&name)?;
        }
        log::debug!("preparing plugin {name}");

        for dependency in self.plugins[name].manifest.dependecies.clone() {
            // prepare plugin only if it is resolved or is not optional
            if !dependency.optional && self.plugins.contains_key(name) {
                if let Some(source) = &dependency.source {
                    self.prepare_plugin(source)?;
                }
            }
        }

        let data = &mut self.plugins.get_mut(name).unwrap();

        let mut handler = std::mem::take(&mut data.handler);
        handler.prepare_plugin(data)?;
        data.handler = handler;
        self.prepared_plugins.insert(name.clone());
        return Ok(());
    }

    pub fn get_prepared_plugins(&self) -> &HashSet<url::Url> {
        return &self.prepared_plugins;
    }
}

#[cfg(test)]
mod test {
    use crate::PluginData;
    use anyhow::*;
    use std::path::{PathBuf, absolute};
    use url::Url;

    #[test]
    fn manifest_load() -> Result<()> {
        let url = Url::parse("core://runtime")?;
        PluginData::resolve(&url)?;
        return Ok(());
    }

    #[test]
    fn path_manifest_load_from_path_url() -> Result<()> {
        let rel_path = PathBuf::from("./plugins/runtime");
        let abs_path = absolute("./plugins/runtime")?;

        let abs_url = url::Url::parse(&format!("path://{}", abs_path.display()))?;
        let abs_plugin = PluginData::resolve(&abs_url)?;

        let rel_url = url::Url::parse(&format!("path://{}", rel_path.display()))?;
        let rel_plugin = PluginData::resolve(&rel_url)?;

        assert_eq!(abs_plugin.root_path, abs_path);
        assert_eq!(rel_plugin, abs_plugin);
        Ok(())
    }
}
