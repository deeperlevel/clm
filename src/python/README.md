# CLM in Python

This project contains the Python implementation of Cross Language Modules (CLM). It uses the Python standard library for its test runner and installs project dependencies from `requirements.txt`.

## Python tooling

Install a current Python 3 release from [python.org/downloads](https://www.python.org/downloads/). On Windows, enable the option to add Python to `PATH` during installation; the Python Launcher (`py`) is also useful for selecting an installed version. On macOS or Linux, use the official installer or your operating system's package manager, and make sure the `venv` module is available.

Install [Visual Studio Code](https://code.visualstudio.com/Download) if it is not already installed.

## VS Code setup

Open the `src/python` directory as the VS Code workspace, then install these extensions:

- [Python](https://marketplace.visualstudio.com/items?itemName=ms-python.python) for Python workspace support and interpreter selection.
- [Pylance](https://marketplace.visualstudio.com/items?itemName=ms-python.vscode-pylance) for language analysis, navigation, and completions.
- [Python Debugger](https://marketplace.visualstudio.com/items?itemName=ms-python.debugpy) for breakpoints and interactive debugging.

Create the virtual environment below, then run **Python: Select Interpreter** from the Command Palette and choose the interpreter inside `.venv`. To debug, open a Python file, set a breakpoint, and start **Run and Debug** with the Python Debugger configuration.

## Virtual environment

Run these commands from `src/python`:

```sh
python -m venv .venv
```

If `python` is not the Python 3 command on your system, use `python3 -m venv .venv` on macOS/Linux or `py -3 -m venv .venv` on Windows.

Activate the environment:

```powershell
# Windows PowerShell
.\.venv\Scripts\Activate.ps1
```

```bat
:: Windows Command Prompt
.venv\Scripts\activate.bat
```

```sh
# macOS/Linux
source .venv/bin/activate
```

Upgrade pip and install the project dependencies after activation:

```sh
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

To leave the virtual environment, run `deactivate`.

## Tests

Run the CLM test suite from `src/python` with the virtual environment's interpreter:

```powershell
.\.venv\Scripts\python.exe __main__.py
```

```sh
./.venv/bin/python __main__.py
```

The runner prints colored assertion results and overwrites `__main__.log` with plain text in the current working directory. It exits with a nonzero status if an assertion fails or the log cannot be written.

## BPA memory model

`clm.bpa` stores fixed-width payloads in Python-managed `bytearray` objects and string payloads as `bytes`. Python handles allocation and reclamation; the C slab allocator and its per-class pool statistics are not recreated. Borrowed payloads are exposed as `memoryview` objects, which keep storage alive and prevent resizing while a view is active. The BPA IDs and reference counts remain explicit to preserve the C API's resource lifecycle.

`clm.dictionary`, `clm.id`, `clm.string`, and `clm.storage` build on BPA/BPAA resources. Dictionary keys and values, ID names and indexes, string contents and arrays, and grid/atlas payloads all use the shared BPA storage layer. Functions that write through C output pointers return Python tuples containing the status and result instead.