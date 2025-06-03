# EasyGameChat

[![PyPI version](https://badge.fury.io/py/easygamechat.svg)](https://pypi.org/project/easygamechat/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/license/mit/)


**EasyGameChat** is a minimal, secure, and cross-platform chat server and client library designed for real-time communication in games or applications. It aims to provide simple, fast integration across multiple languages with a JSON-based protocol and message validation system.


## Features

* Lightweight and efficient server implementation in Go
* Secure and strict input validation
* Modular server-client architecture
* Cross-language support (C++, Python, and more to come)
* JSON-based protocol for easy parsing
* Thread-safe and rate-limited message broadcasting

## Getting Started

### Server (Go)

1. Install Go 1.18 or later.
2. Run the server:

```bash
go run main.go
```

### Client (C++)

1. Include `EasyGameChat.h` in your project.
2. Install the required dependencies (only `nlohmann/json` for JSON parsing, for now) with your preferred package manager. On Windows, you can specify your preferred toolchain in the CMake build command with `-DCMAKE_TOOLCHAIN_FILE=path/to/your/toolchain.cmake`.
3. Compile and run the example:

```bash
cd examples/c-cpp
cmake -B build -S .
# If using vcpkg, specify the toolchain file:
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build
./build/chat-client
```

### Client (Python)

```bash
cd clients/python
python -m build
python -m pip install .\dist\easy_game_chat-0.1.0-py3-none-any.whl
# or install directly from the source
pip install .
python examples/python/main.py
```


## Protocol

Messages follow a simple JSON format:

```json
{
  "from": "nickname",
  "text": "message content"
}
```

All input is sanitized and validated server-side to prevent injection, malformed data, or abuse.


## License

This project is licensed under the MIT License.