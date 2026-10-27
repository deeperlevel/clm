# CLM in C++

This project uses CMake to build the `main` executable with the Clang C++
compiler and the Ninja build system. CMake writes generated files to `build`.

## Toolchain

On Windows, install:

- **LLVM/Clang** from the [LLVM releases](https://github.com/llvm/llvm-project/releases). Make sure `clang++` is on `PATH`.
- **CMake 4.3 or newer** from [cmake.org](https://cmake.org/download/).
- **Ninja** from the [Ninja releases](https://github.com/ninja-build/ninja/releases), and add it to `PATH`.
- **Visual Studio 2022 Build Tools**, including the C++ build tools and a Windows SDK. Clang uses these to find the Windows headers, libraries, and linker.

For VS Code, install the **CMake Tools** and **Microsoft C/C++** extensions, and
the [CodeLLDB extension](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb)
to debug the executable. CMake Tools provides the `cmake` task type used by
`.vscode/tasks.json`. Open VS Code from a **Developer PowerShell for VS 2022**
so Clang can use the installed MSVC linker and SDK.

Verify that `cmake`, `ninja`, and `clang++` are available in the terminal:

```powershell
cmake --version
ninja --version
clang++ --version
```

## Build and run

From this directory, configure and build with:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target main
```

The project enables only the C++ language, so CMake won't search `PATH` for an
unneeded C compiler. If `build` was previously configured with another
compiler, remove `build/CMakeCache.txt` before reconfiguring so CMake can
detect Clang cleanly.

Run the resulting executable:

```powershell
.\build\main.exe
```

In VS Code, **CMake: Build** configures and builds the `main` target, and
**Launch main (Clang)** builds it before starting the debugger.
