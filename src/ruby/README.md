# CLM in Ruby

This project uses Ruby. The code uses `Data.define`, so Ruby 3.2 or later is required. Use a currently maintained Ruby release; this project does not pin a specific version.

## Ruby installation

### Windows

Install the x64 **Ruby+Devkit** package from [RubyInstaller for Windows](https://rubyinstaller.org/downloads/). The Devkit provides MSYS2 tools used when gems need native extensions. Follow the installer prompts to complete the MSYS2 setup.

### macOS and Linux

Use the installation method for your operating system from the official [Ruby installation guide](https://www.ruby-lang.org/en/documentation/installation/). Version managers such as [rbenv](https://github.com/rbenv/rbenv) are useful when you need more than one Ruby version; avoid relying on the system Ruby on macOS.

Open a new terminal and verify that Ruby and RubyGems are available:

```sh
ruby --version
gem --version
bundle --version
```

Bundler is Ruby's dependency manager. This project currently has no `Gemfile` or external gem dependencies, so there is nothing to install with Bundler.

## VS Code setup

Open the `src/ruby` directory as the VS Code workspace, then install:

- [Ruby LSP](https://marketplace.visualstudio.com/items?itemName=Shopify.ruby-lsp) for Ruby language features such as navigation, completion, diagnostics, and formatting support.
- [VSCode rdbg Ruby Debugger](https://marketplace.visualstudio.com/items?itemName=KoichiSasada.vscode-rdbg) for breakpoints, stepping, and variable inspection with Ruby's `rdbg` debugger.

## Run the program

From the `src/ruby` directory, run:

```sh
ruby main.rb
```

The entry point runs all seven module test suites and writes plain test output to `main.log`.

## CLM Modules

`clm/base.rb` provides mutable records for positions, sizes, and blocks in 2D and 3D, with the numeric suffix aliases from `base.h`. The default names (`CLM::Base::Position`, `CLM::Base::Size`, and so on) refer to the 32-bit signed variants. Ruby integers and floats are not fixed-width, so these records do not enforce C storage sizes or provide binary-compatible layouts. `CLM::Base.valid_id?` checks whether an integer fits the unsigned 64-bit `clm_id` range.

`clm/console.rb` exposes the console operations as procedural `CLM::Console` module methods. It preserves the C status values (`SUCCESS`, `FAILURE`, and `INVALID_ARGUMENT`) and ANSI control sequences. Ruby stream methods take `IO`-like objects; input methods that use C output pointers return `[status, value]` pairs. Line and character input are byte-oriented and return binary strings.

`clm/bpa.rb` provides block and array storage through `CLM::BPA`. It uses Ruby-managed values, binary strings, and arrays instead of a native allocation pool. Built-in numeric types validate their value ranges; `TYPE_STRUCT` stores copied Ruby objects, with `block_size` retained as metadata for compatible transfers rather than a Ruby memory layout. Array capacity and sealing are explicit metadata. Borrowed arrays are Ruby handles that keep their storage reachable; they are not raw pointers. The C pool statistics and byte-buffer APIs have no direct Ruby equivalent.

`clm/dictionary.rb` provides typed `CLM::Dictionary` mappings with insertion-order iteration. Keys, values, and metadata are stored in BPA blocks and arrays; strings are copied into BPA string arrays, while `TYPE_STRUCT` values use Ruby-managed copies.

`clm/id.rb` provides `CLM::ID` name registration, lookup, update/removal, aliases, ASCII-only case-insensitive mode, and sequential ID generation. Its registry is built on `CLM::Dictionary` and BPA storage.

`clm/string.rb` provides `CLM::String` editing by Unicode code point. Text is stored in BPA string blocks/arrays as validated UTF-8; byte import/export supports UTF-8, Latin-1, and UTF-16LE. Ruby returns strings and `[status, value]` pairs instead of C caller buffers; borrowed string slices are safe Ruby string values.

`clm/storage.rb` provides BPA-backed row-major grids and atlases through `CLM::Storage`. Coordinates are zero-based. Ruby methods return values or arrays rather than copying into caller-provided byte buffers.

Run the test suites with:

```sh
ruby main.rb
```

The test helper prints colored results through `CLM::Console` and writes plain output to `main.log` in the current working directory. It truncates the log at the start of each run and flushes each output line. A log open, write, or close failure makes the test command fail. The same suite is available as **Terminal > Run Task > Ruby: Test** in VS Code.

## Debugging

The `debug` gem provides `rdbg`. It is included with some Ruby installations; if the `rdbg` command is unavailable, install the gem:

```sh
gem install debug
```

To start a terminal debugging session:

```sh
rdbg main.rb
```

In VS Code, open `main.rb`, set a breakpoint, then press **F5**. The rdbg extension can prompt for the command to run; enter `ruby main.rb`. You can also add `binding.break` in Ruby code to stop at a specific line when running under the debugger.

## Project layout and tests

- `main.rb` runs all seven test suites.
- `clm/` contains the Ruby module implementations for Base, Console, BPA, Dictionary, ID, String, and Storage.
- `tests/test_clm_base.rb` covers the base records and ID validation.
- `tests/test_clm_console.rb` covers console output, input, and ANSI operations.
- `tests/test_clm_bpa.rb` covers BPA blocks, arrays, transfers, slices, and borrows.
- `tests/test_clm_dictionary.rb` covers typed values, ordered keys, mutation, and BPA block transfers.
- `tests/test_clm_id.rb` covers name mapping, case behavior, aliases, and ID generation.
- `tests/test_clm_string.rb` covers Unicode editing, encoding conversion, slices, and BPA-backed string arrays.
- `tests/test_clm_storage.rb` covers row-major grids, slices, and atlas frames.