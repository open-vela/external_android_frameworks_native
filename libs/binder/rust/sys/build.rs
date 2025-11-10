/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use std::env;
use std::path::PathBuf;

fn main() {
    let target_os =
        env::var("CARGO_CFG_TARGET_OS").expect("TARGET_OS environment variable not found");
    if target_os != "nuttx" {
        return;
    }

    let mut builder = bindgen::Builder::default();

    let nuttx_apps_dir_env = env::var("NUTTX_APPS_DIR").expect("NUTTX_APPS_DIR not set");
    let nuttx_apps_dir = PathBuf::from(nuttx_apps_dir_env);
    let nuttx_include_dir_env = env::var("NUTTX_INCLUDE_DIR").expect("NUTTX_INCLUDE_DIR not set");
    let nuttx_include_dirs: Vec<&str> = nuttx_include_dir_env.split(':').collect();
    for nuttx_include_dir in nuttx_include_dirs {
        let nuttx_include_dir = nuttx_include_dir.trim();
        if !nuttx_include_dir.is_empty() {
            builder = builder.clang_arg(format!("-I{nuttx_include_dir}"));
        }
    }
    let newlib_include = nuttx_apps_dir.join("../nuttx/libs/libm/newlib/newlib/newlib/libc/include");
    let binder_path = nuttx_apps_dir.join("external/android/frameworks/native/libs/");
    let ndk_include = binder_path.join("binder/ndk/include_ndk");
    let ndk_include_platform = binder_path.join("binder/ndk/include_platform");

    let bindings = builder
        .clang_arg(format!("-I{}", newlib_include.display()))
        .clang_arg(format!("-I{}", ndk_include.display()))
        .clang_arg(format!("-I{}", ndk_include_platform.display()))
        // TODO figure out what the "standard" #define is and use that instead
        .header("BinderBindings.hpp")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Keep in sync with libbinder_ndk_bindgen_flags.txt
        .default_enum_style(bindgen::EnumVariation::Rust {
            non_exhaustive: true,
        })
        .constified_enum("android::c_interface::consts::.*")
        .allowlist_type("android::c_interface::.*")
        .allowlist_type("AStatus")
        .allowlist_type("AIBinder_Class")
        .allowlist_type("AIBinder")
        .allowlist_type("AIBinder_Weak")
        .allowlist_type("AIBinder_DeathRecipient")
        .allowlist_type("AParcel")
        .allowlist_type("binder_status_t")
        .blocklist_function("vprintf")
        .blocklist_function("strtold")
        .blocklist_function("_vtlog")
        .blocklist_function("vscanf")
        .blocklist_function("vfprintf_worker")
        .blocklist_function("vsprintf")
        .blocklist_function("vsnprintf")
        .blocklist_function("vsnprintf_filtered")
        .blocklist_function("vfscanf")
        .blocklist_function("vsscanf")
        .blocklist_function("vdprintf")
        .blocklist_function("vasprintf")
        .blocklist_function("strtold_l")
        .allowlist_function(".*")
        .generate()
        .expect("Couldn't generate bindings");
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings.");
    println!("cargo::rustc-link-lib=binder_ndk");
}
