# CLM in Go

This is the Go implementation of Cross Language Modules (CLM). It includes
base types, block pool allocation (BPA), console helpers, dictionaries, an ID
registry, Unicode strings, and grid/atlas storage.

The modules declare **Go 1.25.4** as their minimum version. Install the latest
stable Go release for current editor and debugger support. This implementation
uses the Go standard library; it does not require CMake, Ninja, a C compiler,
or the C implementation to build.

## Toolchain Downloads

Download Go for your operating system and CPU architecture from
[go.dev/dl](https://go.dev/dl/). Follow the official
[installation instructions](https://go.dev/doc/install).

### Windows

1. Install Go using the Windows MSI installer. The usual installation directory
   is `C:\Program Files\Go`; a custom directory such as `C:\dev\Go` also works.
2. Ensure the installation's `bin` directory is on `PATH`.
3. Close and reopen VS Code and any terminals after installation so they pick
   up the updated environment.

Verify the installation in PowerShell:

```powershell
go version
Get-Command go
go env GOROOT GOPATH
```

`GOROOT` identifies the Go installation. `GOPATH` is a separate location for
downloaded modules and installed developer tools; do not set it to `GOROOT`.

### Linux

Install Go using the official Linux archive and installation instructions, or
your distribution's package manager if it provides a sufficiently recent
version. Distribution packages can lag behind the current stable release.

For an official installation under `/usr/local/go`, add its binaries to your
shell's `PATH`:

```sh
export PATH="/usr/local/go/bin:$PATH"
go version
go env GOROOT GOPATH
```

Make the `PATH` change persistent in the appropriate startup file for your
shell. When replacing an existing official installation, follow Go's removal
and installation steps rather than extracting a new archive over an old tree.

### macOS

Install the official macOS package for Apple Silicon (`arm64`) or Intel
(`amd64`), as appropriate. Reopen your terminal and verify with `go version`.

## VS Code Setup

Install [Visual Studio Code](https://code.visualstudio.com/download), then
install the official [Go extension](https://marketplace.visualstudio.com/items?itemName=golang.go)
from the Go Team at Google. Its extension ID is `golang.go`.

The Go extension provides Go editing, testing, and debugging integration. No
CMake Tools, C/C++, or CodeLLDB extension is required for this Go project.

1. Open the **`src/go` directory** as the VS Code workspace, not just an
   individual source file or the `cmd` directory.
2. Open a Go source file and accept the extension's prompts to install tools.
3. Open the Command Palette with `Ctrl+Shift+P` (`Cmd+Shift+P` on macOS), run
   **Go: Install/Update Tools**, and install or update:
   - **`gopls`**, the Go language server for completion, navigation, formatting,
     and diagnostics.
   - **`dlv`**, the Delve debugger used by the Go extension.
4. Run **Go: Locate Configured Go Tools** to check the selected SDK and tools.

Tool installation requires network access. By default, `go install` places
executables under `GOPATH/bin`, commonly `%USERPROFILE%\go\bin` on Windows or
`$HOME/go/bin` on Linux/macOS, unless `GOBIN` is configured. Add that directory
to `PATH` if you want to invoke those tools directly from a terminal.

### SDK Paths In This Checkout

The checked-in [.vscode/settings.json](.vscode/settings.json) selects
`C:\dev\Go` through `go.goroot` and `go.alternateTools.go`.
The Windows overrides in [.vscode/tasks.json](.vscode/tasks.json) also use
executables from `C:\dev\Go\bin`.

**Adjust these paths if Go is installed elsewhere.** On Linux/macOS, remove
the Windows-specific SDK settings from the workspace settings file. If Go is
correctly discoverable on `PATH`, you can remove the explicit SDK settings and
the Windows task overrides and let the Go extension and tasks use `go` and
`gofmt` from the environment.

Changing `go.alternateTools` configures VS Code; it does not add Go to your
terminal's `PATH`. After changing SDK settings, run **Go: Restart Language
Server**. If necessary, run **Developer: Reload Window** as well.

## Project Layout

- [cmd/main.go](cmd/main.go): executable that runs all seven CLM test suites.
- [pkg/clm](pkg/clm): reusable Go implementations.
- [tests](tests): shared assertions, executable suites, and standard Go tests.
- [go.mod](go.mod): root module and local dependency on the tests module.
- [tests/go.mod](tests/go.mod): separate test module with a local root dependency.
- [go.work](go.work): includes both modules in one Go workspace.

Keep the workspace file enabled when developing this checkout. Local module
replacements resolve the library and tests without publishing either module.

## Build, Run, And Test

Run the following commands from **`src/go`**.

### Run The Test Executable

```sh
go run ./cmd
```

The runner executes Base, BPA, Console, Dictionary, ID, String, and Storage
suites in the same order as the C runner, then prints a combined summary.
It prints colored assertions to the terminal and writes plain output to
`main.log` in the current working directory. The log is overwritten at the
start of a run. The runner checks assertion failures and log open/write/close
failures when selecting its exit status.

### Build An Executable

On Windows:

```powershell
go build -o clm-tests.exe ./cmd
.\clm-tests.exe
```

On Linux/macOS:

```sh
go build -o clm-tests ./cmd
./clm-tests
```

Go manages its own build cache. There is no shared CMake build directory to
configure, and these commands place the executable in the current directory.

### Run Standard Go Tests

```sh
go test ./... ./tests/... -count=1
```

Specify both patterns: `./...` alone does not recurse into the separate tests
module. `-count=1` disables test-result caching. To run one suite:

```sh
go test ./tests/... -run '^TestString$' -count=1
```

The test package also writes its own `main.log` relative to its working
directory, normally under `tests` when invoked with `go test`.

### Format Source

```sh
gofmt -w cmd pkg/clm tests
```

## VS Code Tasks And Debugging

### Tasks

Use **Terminal > Run Task** to select a checked-in task:

- **Run cmd/main.go** runs the combined test executable with `go run`.
- **Validate Go Port** runs standard Go tests for the root and tests modules.
- **Format Go Port** formats the Go sources using `gofmt`.

There is currently no default build task. Use the build command above to
produce a standalone executable.

### Debugger

The checked-in [.vscode/launch.json](.vscode/launch.json) provides
**Debug cmd/main.go**. Install `dlv` through **Go: Install/Update Tools**, set a
breakpoint, select this configuration in **Run and Debug**, and press `F5`.
The Go extension builds the program for debugging; no separate C build task
or CodeLLDB configuration is needed.

Standard Go tests can also be run or debugged using the links above test
functions in [tests/suite_test.go](tests/suite_test.go), or from VS Code's
Testing view.

## Troubleshooting

- **`go` is not recognized:** check the SDK's `bin` directory is on `PATH`,
  then reopen VS Code and terminals. On Windows, use `Get-Command go` to check
  which executable is selected.
- **Tasks reference a missing executable:** update or remove the hard-coded
  Windows paths in the task configuration. Editor SDK settings do not override
  an explicit task command.
- **"No packages found" or "No active builds contain...":** open `src/go` as
  the workspace, verify the SDK paths, and run **Go: Restart Language Server**.
  Check `go env GOMOD GOWORK`: the paths should identify this checkout's
  module and workspace files. Make sure `GOWORK` is not set to `off` or an
  unrelated workspace.
- **Tests are missing from `go test ./...`:** include `./tests/...` explicitly
  and keep both modules listed in the workspace file.
- **Debugger/tool version errors after a Go upgrade:** use
  **Go: Install/Update Tools** to update `gopls` and `dlv`, then restart the
  language server.
- **ANSI escape sequences appear in captured output:** use an ANSI-compatible
  terminal or inspect the plain-text `main.log` instead.

## Go API Conventions

Exported names use PascalCase without the redundant C `clm_` prefix. For
example, `clm_string_new` becomes `clm.StringNew` after importing
`github.com/deeperlevel/clm/src/go/pkg/clm`.

Buffer APIs accept byte slices rather than raw pointers plus separate sizes.
Many C output parameters become `(value, status)` returns. BPA status values
are retained: failure `-1`, success `0`, get success `1`, and set success `2`.
Boolean-returning APIs retain boolean results. Use `encoding/binary` when
converting fixed-width numeric values to or from byte buffers; managed numeric
metadata transfers use little-endian bytes.

### Base Types

[pkg/clm/base.go](pkg/clm/base.go) provides 64-bit IDs, fixed-width numeric
aliases, and generic 2D/3D position, size, and block types. Concrete aliases
cover signed/unsigned integers and floating-point values; default geometry
uses `int32` fields.

### BPA

[pkg/clm/bpa.go](pkg/clm/bpa.go) provides ID-addressed blocks and arrays,
reference counting, fixed-width structs, byte strings, slicing, cursors,
sorting, drawing, and retained advanced array handles. Scalar allocations
reuse zeroed size-class slabs; arrays use Go-managed memory.

Sealing an array prevents capacity changes and growth, not mutations that fit
within its reserved capacity. BPA byte-string buffers include a NUL
terminator. Borrowed slices alias the source and must not be retained across
replacement, resizing, deletion, or shutdown. Call `BpaShutdown` when finished
with the global registries; it invalidates outstanding resource handles.

### Dictionaries

[pkg/clm/dictionary.go](pkg/clm/dictionary.go) provides typed integer-key
dictionaries with copied values and insertion-order iteration. `Set` inserts
or replaces, `AddFrom` requires an absent key, and `UpdateFrom` requires an
existing key. Replacing a value preserves order; removing and reinserting a
key moves it to the end. Low-level string values are bounded NUL-terminated
bytes, not automatically validated Unicode text.

### ID Registry

[pkg/clm/id.go](pkg/clm/id.go) registers UTF-8 names for numeric IDs, supports
aliases and removal, and generates sequential IDs starting at zero. Matching
is case-sensitive by default; optional case-insensitive lookup folds ASCII
letters only. `IDTryGet` reports status separately so the not-found sentinel
can also be stored as an ordinary mapped value.

### Strings

[pkg/clm/string.go](pkg/clm/string.go) validates and stores text internally as
UTF-8. Indexes and lengths count Unicode code points, not bytes or grapheme
clusters. Export encodings are UTF-8, Latin-1, and UTF-16LE. UTF-16LE exports
have no NUL terminator. String arrays support indexed text edits, searching,
sorting, cursors, and deck draws that reshuffle when exhausted.

### Console

[pkg/clm/console.go](pkg/clm/console.go) uses standard input/output and Go
`io.Reader`/`io.Writer` interfaces. It emits ANSI sequences for terminal
controls and colors. Line input accepts at most 4095 bytes, drains an overlong
line before the next read, and returns independently owned Go strings.
Character input may still require Enter in an interactive terminal's normal
line mode. Timed text effects use Go timers rather than a C busy wait.

### Storage

[pkg/clm/storage.go](pkg/clm/storage.go) provides zero-initialized fixed-width
2D/3D grids and layered atlases. Coordinates are passed in layer, row, column
order; columns vary fastest in row-major storage. Region and frame slices
own independent copies. Variable-width strings are not supported as cells.

The global registries and runtime defaults are **not internally synchronized**.
Synchronize concurrent access in the caller, including mutation versus reads.
Unlike C headers, Go packages do not need implementation preprocessor defines.