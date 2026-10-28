# CLM in D

This directory contains the D implementation of CLM and its test runner. The
recommended compiler is **LDC**, whose command-line executable is **`ldc2`**.
The project builds directly from D source files; CMake, Ninja, and a DUB project
configuration are not required.

The checked-in VS Code tasks target **Windows x64**. Linux and macOS users can
build from the command line, but must adapt the Windows-specific tasks and
debugger paths before using them.

## Toolchain downloads

### Windows

1. Download a stable Windows x64 installer or binary archive from the
   [LDC releases page](https://github.com/ldc-developers/ldc/releases/latest).
   This port has been compiled with LDC 1.43.0; this is not a declared minimum
   supported version.
2. Install or extract the complete distribution, for example to `C:\dev\ldc`.
   Keep its libraries, imports, and configuration alongside the compiler; do
   not copy only `ldc2.exe`.
3. Add its `bin` directory, for example `C:\dev\ldc\bin`, to your user `PATH`.
   Restart VS Code and open a new terminal after changing `PATH`.
4. If your LDC distribution requires Microsoft linker or SDK components,
   install [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/)
   with **Desktop development with C++**, including the x64 MSVC tools and a
   Windows SDK. Follow the selected LDC release's installation instructions:
   some distributions bundle the libraries and linker they need.

Check the compiler in PowerShell:

```powershell
Get-Command ldc2
ldc2 --version
```

LDC is LLVM-based, but its official binary distribution does not require you to
build LLVM or install a separate Clang compiler just to compile this project.
LLDB is a separate debugging tool; see below.

### Linux and macOS

Use the official [LDC binary releases](https://github.com/ldc-developers/ldc/releases)
or your package manager. For example:

```sh
# Debian / Ubuntu
sudo apt update
sudo apt install ldc lldb build-essential

# macOS with Homebrew
brew install ldc
xcode-select --install
```

On macOS, the Xcode Command Line Tools provide the platform SDK and LLDB.
On Linux, retain the native linker and C development libraries needed by LDC.
Distribution packages may lag behind the official LDC releases.

## VS Code setup

Install [Visual Studio Code](https://code.visualstudio.com/), then these
extensions:

| Extension | Purpose |
|-----------|---------|
| [D Programming Language (code-d)](https://marketplace.visualstudio.com/items?itemName=webfreak.code-d) | D completion, navigation, formatting, and language-server integration. |
| [CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb) | LLDB debugging using launch configurations with `"type": "lldb"`. |
| [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) | Required for the existing Windows **Debug D** configuration, which uses `"type": "cppvsdbg"`, not LLDB. |

You can install them from the Extensions view (`Ctrl+Shift+X`), or with:

```powershell
code --install-extension webfreak.code-d
code --install-extension vadimcn.vscode-lldb
code --install-extension ms-vscode.cpptools
```

code-d can download its supporting tools, including `serve-d` and DCD, if they
are missing. Allow its setup to complete when prompted. DUB may be used by
editor tooling, but is not this project's build command.

Open **this directory** (`src\d`) as the VS Code workspace, rather than the
repository root. `${workspaceFolder}` must point to the directory containing
[main.d](main.d) and [.vscode](.vscode).

## Building and running

### VS Code tasks

[.vscode/tasks.json](.vscode/tasks.json) contains:

- **Build D (debug)**: runs the clean and compile tasks in sequence.
- **Clean D outputs**: creates `bin` if necessary and removes the named main
  executable, object, and PDB outputs. It does not recursively delete `bin`.
- **Compile D (debug)**: invokes `ldc2 -i main.d -od=bin -of=bin/main.exe -g`.

Use **Terminal > Run Task > Build D (debug)**. The existing debugger also invokes
this task automatically before launching. Stop an active debugging session
before rebuilding so Windows can replace the executable.

`-i` compiles imported project modules, and `-g` emits debug information. No
separate compilation list is needed for the normal build.

### PowerShell

Run these commands from this directory:

```powershell
New-Item -ItemType Directory -Path bin -Force | Out-Null
ldc2 -i main.d -od=bin -of=bin\main.exe -g
if ($LASTEXITCODE -eq 0) { .\bin\main.exe }
```

To check compilation without linking an executable:

```powershell
ldc2 -c -i main.d -od=bin -g
```

For an explicit all-module compilation check, use qualified object names to
avoid collisions between the two package modules:

```powershell
ldc2 -c main.d clm\*.d tests\*.d -oq -od=bin -g
```

### Linux / macOS command line

From this directory:

```sh
mkdir -p bin
ldc2 -i main.d -od=bin -of=bin/main -g
./bin/main
```

## LLDB debugger

### Where to get LLDB

For VS Code, **CodeLLDB is the simplest installation route**: its platform
package includes an LLDB backend. A separate `lldb` executable on `PATH` is not
normally needed by the extension.

For command-line debugging, obtain LLDB through:

- **Windows:** an [LLVM release](https://github.com/llvm/llvm-project/releases)
  containing LLDB for Windows. Verify that the selected package actually includes
  `lldb.exe`, then add its `bin` directory to `PATH`.
- **Linux:** your distribution's `lldb` package.
- **macOS:** the Xcode Command Line Tools (`xcode-select --install`).

Check a standalone installation with `lldb --version`.
CodeLLDB's bundled backend does not necessarily install a global `lldb` command.

### Existing Windows debugger

The checked-in [.vscode/launch.json](.vscode/launch.json) provides **Debug D**.
It uses Microsoft's Windows debugger through the C/C++ extension, launches
`bin\main.exe`, runs **Build D (debug)** first, and uses the workspace directory
as its working directory. Select **Debug D** in **Run and Debug**, set a
breakpoint in [main.d](main.d), and press `F5`.

This is compatible with LDC's default Windows CodeView/PDB debug information.
Installing CodeLLDB alone does not change this configuration to LLDB.

### Optional CodeLLDB configuration

To use LLDB instead, add this object to the `configurations` array in
[.vscode/launch.json](.vscode/launch.json), keeping the existing entry if desired:

```json
{
  "name": "Debug D (LLDB)",
  "type": "lldb",
  "request": "launch",
  "program": "${workspaceFolder}\\bin\\main.exe",
  "cwd": "${workspaceFolder}",
  "preLaunchTask": "Build D (debug)",
  "terminal": "integrated"
}
```

Select **Debug D (LLDB)** before pressing `F5`. On Linux or macOS, use the native
`bin/main` executable and a platform-appropriate build task instead.

LLDB supports Windows PDB information, but its support is less complete than
DWARF. If symbols or source stepping do not work well, try LDC's `-gdwarf` flag
alongside `-g` in the compile task and rebuild before using CodeLLDB. Keep a
separate build task if you also want to retain the Microsoft/PDB workflow.
D-specific expression evaluation and display of arrays, strings, or classes
can be limited; do not assume C++ visualizers provide full D language support.

See the [CodeLLDB manual](https://github.com/vadimcn/codelldb/blob/master/MANUAL.md)
and [Windows notes](https://github.com/vadimcn/codelldb/wiki/Windows) for debugger
configuration and debug-information limitations.

## Tests and log output

[main.d](main.d) runs the base, BPA, console, dictionary, ID, string, and storage
suites. Assertions are printed in color through the D console module and also
written as plain text to `main.log` in the **current working directory**.

The log is overwritten at the start of each run. With the provided debugger
configuration or the commands above, it is written next to this README, not
inside `bin`. Running the executable with `bin` as the working directory writes
the log there instead.

The runner returns a nonzero exit code for failed assertions or errors opening,
writing, or closing the log, printing console output, or shutting down BPA.

## Project layout

- [clm](clm): library modules; `import clm;` exposes the package's public modules.
- [tests](tests): test suites and shared colored-output/logging helpers.
- [main.d](main.d): test runner and overall summary.
- [.vscode](.vscode): Windows build and debugger configuration.
- `bin`: generated build outputs.

## Troubleshooting

- **`ldc2` is not recognized:** check `Get-Command ldc2`, fix `PATH`, and restart
  VS Code. Its processes retain the environment from when it was launched.
- **Missing linker, runtime, or Windows SDK libraries:** check that the full LDC
  distribution is intact and install its documented Windows prerequisites.
- **Unknown debugger type `cppvsdbg` or `lldb`:** install the C/C++ or CodeLLDB
  extension respectively, and select the intended launch configuration.
- **Executable cannot be overwritten:** stop the debugger and any running test
  executable before building.
- **Breakpoints remain unbound:** rebuild with `-g`, check that the launch path
  matches the newly built executable, and review the PDB/DWARF guidance above.
- **Compiler crashes with `0xC000001D`:** this is an illegal-instruction crash,
  not a D source diagnostic. Compare the compile-only command with the full
  build, record `ldc2 --version` and the failing command, and investigate the
  compiler installation and toolchain. Do not treat successful compilation
  alone as proof that executable linking or runtime tests succeeded.
