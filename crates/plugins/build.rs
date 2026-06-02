use std::path::{Component, PathBuf};

use pkg_config::Library;

fn make_bind(lua_config: &Library, path: &str, blocks: &[&str], allows: &[&str]) {
    let real_path = PathBuf::from(path);
    let mut fake_path = String::new();
    for comp in real_path.components() {
        fake_path += "_";
        fake_path += match comp {
            Component::Normal(ostr) => ostr.to_str().unwrap(),
            Component::Prefix(_) => "",
            Component::ParentDir => "__",
            Component::CurDir => "",
            Component::RootDir => "/",
        }
    }

    let mut bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header(path)
        .impl_debug(true)
        .impl_partialeq(true)
        .derive_copy(true);

    for lib in &lua_config.include_paths {
        bindings = bindings.clang_arg(format!("-I{}", lib.display()));
    }

    for allow in allows {
        bindings = bindings.allowlist_item(allow);
    }

    for block in blocks {
        bindings = bindings.blocklist_item(block);
    }

    let bindings = bindings
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    // Write generated bindings into Cargo's output directory so rustc never
    // races with a source file being rewritten in-place.
    let out_path = PathBuf::from(std::env::var("OUT_DIR").expect("OUT_DIR is not set"))
        .join(format!("_{fake_path}_raw_bindings.rs"));
    bindings
        .write_to_file(out_path)
        .expect("Couldn't write bindings!");
}

fn main() {
    let lua_config = pkg_config::Config::new()
        .atleast_version("5.4")
        .probe("lua")
        .unwrap();
    println!("cargo:rerun-if-changed=../../include/plugin.h");
    println!("cargo:rerun-if-changed=../../include/plugin_bindings.h");
    println!("cargo:rerun-if-changed=../../include/plugin/definitions.h");

    make_bind(
        &lua_config,
        "../../include/plugin_bindings.h",
        &["PluginData", "PluginKind"],
        &[
            "PluginHandler",
            "prepare_lua_plugin",
            "prepare_c_plugin",
            "prepare_binary_plugin",
            "prepare_rust_plugin",
            "unload_lua_plugin",
            "unload_c_plugin",
            "unload_binary_plugin",
            "unload_rust_plugin",
        ],
    );
    make_bind(&lua_config, "../../include/plugin/definitions.h", &[], &[]);
}
