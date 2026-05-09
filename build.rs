use std::path::PathBuf;

fn main() {
    let lua_config = pkg_config::Config::new()
        .atleast_version("5.4")
        .probe("lua")
        .unwrap();
    println!("cargo:rerun-if-changed=include/plugin.h");
    println!("cargo:rerun-if-changed=include/bindgen.h");
    println!("cargo:rerun-if-changed=include/plugin_bindings.h");

    let mut bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header("include/plugin_bindings.h")
        // .opaque_type("lua_State")
        .impl_debug(true)
        .impl_partialeq(true)
        .derive_copy(true);

    for lib in &lua_config.include_paths {
        bindings = bindings.clang_arg(format!("-I{}", lib.display()));
    }

    let blocks = ["PluginData", "PluginKind"];

    let allows = [
        "PluginHandler",
        "load_lua_plugin",
        "load_c_plugin",
        "load_binary_plugin",
        "load_rust_plugin",
        "unload_lua_plugin",
        "unload_c_plugin",
        "unload_binary_plugin",
        "unload_rust_plugin",
    ];

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
        .join("_raw_bindings.rs");
    bindings
        .write_to_file(out_path)
        .expect("Couldn't write bindings!");
}
