use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=include/plugin.h");
    println!("cargo:rerun-if-changed=include/bindgen.h");
    println!("cargo:rerun-if-changed=include/plugin_bindings.h");
    let mut bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header("include/plugin_bindings.h")
        // .opaque_type("lua_State")
        .impl_debug(true)
        .derive_copy(true);

    let blocks = ["PluginData", "PluginKind"];

    let allows = [
        "PluginHandler",
        "load_c_plugin",
        "unload_c_plugin",
        "load_lua_plugin",
        "unload_lua_plugin",
        "load_binary_plugin",
        "unload_binary_plugin",
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
