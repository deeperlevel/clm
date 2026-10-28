# CLM in Zig

This folder contains the Zig implementation of CLM and its console-based test runner. It uses Zig's compiler directly; no separate build system or C compiler is required.

## Zig toolchain

Download a stable Zig release for your operating system and CPU architecture from the [official Zig downloads page](https://ziglang.org/download/). The archive includes the compiler and standard library.

On Windows, extract the archive to a permanent directory such as `C:\dev\zig` and add that directory to `PATH`. On Linux or macOS, extract the matching archive and add its `zig` directory to `PATH`. Restart VS Code after changing `PATH`, then check the installation in a new terminal:

```sh
zig version
```

Use a stable release rather than a development snapshot. The current stable release is listed on the downloads page; keep Zig and ZLS on matching release versions.

## VS Code extensions

Install these extensions from the VS Code Marketplace:

- [Zig Language](https://marketplace.visualstudio.com/items?itemName=ziglang.vscode-zig) for Zig syntax support, formatting, compiler diagnostics, and optional language-server features.
- [CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb) for the LLDB debugger used by the checked-in launch configuration.

ZLS (Zig Language Server) is optional. It adds editor features such as completions and go-to-definition. Install the release matching your Zig version from [ZLS releases](https://github.com/zigtools/zls/releases), or follow the Zig extension's setup prompts. Tagged Zig and ZLS releases should use the same version.

## Build and run

Open this `src/zig` directory as the VS Code workspace so the checked-in task paths resolve correctly. Make sure `zig` is available on `PATH`.

The default **Build Zig** task (run with **Terminal > Run Build Task** or `Ctrl+Shift+B`) creates `build/` and compiles `main.zig` in Debug mode. The executable and its debug symbols are written to `build/main.exe` and `build/main.pdb`. Running the executable executes the base, BPA, console, dictionary, ID, string, and storage test suites, prints their results, and writes plain-text output to `main.log`.

Run the program from PowerShell with:

```powershell
.\build\main.exe
```

To debug, select **Debug Zig** in **Run and Debug**. The launch configuration builds the program first, then starts it with CodeLLDB.

The checked-in VS Code tasks use PowerShell and target a Windows `.exe`. On Linux or macOS, Zig itself is available from the official downloads page, but the checked-in task and launch paths need to be adapted for the host executable format and shell.
