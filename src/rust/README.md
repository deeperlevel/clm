# CLM in Rust

This project is a Rust 2024 binary crate built with Cargo.

## Toolchain downloads

Install Rust with [rustup](https://rustup.rs/). Rustup installs and manages the Rust compiler (`rustc`) and Cargo, Rust's build and package manager. Use the stable toolchain unless the project specifies otherwise.

After installation, verify the tools are available in your terminal:

```sh
rustc --version
cargo --version
```

### Windows

Install the **Visual Studio Build Tools** from [Visual Studio downloads](https://visualstudio.microsoft.com/downloads/), selecting the **Desktop development with C++** workload. This provides the linker and Windows SDK required by Rust's default MSVC toolchain. Install rustup from [rustup.rs](https://rustup.rs/) and choose the `x86_64-pc-windows-msvc` toolchain when prompted.

### Linux

Install a C linker and development tools using your distribution's package manager, then install rustup. For example, on Debian or Ubuntu:

```sh
sudo apt update
sudo apt install build-essential
```

## VS Code setup

Open the `src/rust` directory as the VS Code workspace so `${workspaceFolder}` points to this `Cargo.toml`.

Install these VS Code extensions:

- [rust-analyzer](https://marketplace.visualstudio.com/items?itemName=rust-lang.rust-analyzer) for Rust language support, diagnostics, and Cargo integration.
- [CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb) for the LLDB debugger. The checked-in launch configuration uses the `lldb` debug type provided by this extension.

The checked-in `.vscode/tasks.json` defines **Build Rust** as the default build task. Run it with **Terminal > Run Build Task** (or `Ctrl+Shift+B`); it runs `cargo build` in the workspace folder.

The checked-in `.vscode/launch.json` defines **Debug Rust**. Select it in **Run and Debug** to build and launch the debug executable. Its configured program path is `target\\debug\\clm.exe`, so this launch configuration is for Windows. On Linux, use Cargo commands in the terminal or update the launch configuration's program path to `target/debug/clm` before debugging.

## Cargo commands

Run these commands from this directory:

```sh
cargo build       # compile the project
cargo run         # build and run the binary
cargo test        # run the test suite
cargo fmt --check # check formatting
cargo clippy      # run additional Rust lints
```

Install the formatter and linter components if they are missing:

```sh
rustup component add rustfmt clippy
```