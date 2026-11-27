# EasyGameChat

[![PyPI version](https://badge.fury.io/py/easygamechat.svg)](https://pypi.org/project/easygamechat/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/license/mit/)


**EasyGameChat** is a minimal, secure, and cross-platform chat server and client library designed for real-time communication in games or applications. It aims to provide simple, fast integration across multiple languages with a JSON-based protocol and message validation system.


## Features

* Lightweight and efficient server implementation in Go
* TLS encryption for secure communication
* Secure and strict input validation
* Modular server-client architecture
* Cross-language support (C++, Python, and more to come)
* JSON-based protocol for easy parsing
* Thread-safe and rate-limited message broadcasting

## Getting Started

### Server (Go)

1. Install Go 1.18 or later.
2. Generate a TLS certificte and key with the provided scripts or, preferably, use your own. The server requires a valid certificate and key for secure communication.
3. Run the server:

```bash
go run main.go
```

> TLS is enabled by default. You can configure certificate paths or disable TLS in the config file or server options.

### Client (C++)

Build and install the library:

```bash
cmake -S clients/cpp -B build
cmake --build build
cmake --install build --prefix /path/to/prefix
```

Then link `easygamechat::easygamechat`:

```cmake
find_package(easygamechat CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE easygamechat::easygamechat)
```

Build dependencies are CMake, a C++14 compiler, and `nlohmann/json`; OpenSSL is required on Linux and macOS. Windows uses Schannel.

Run the example:

```bash
cd examples/c-cpp
cmake -B build -S .
cmake --build build
./build/chat_client
```
> Run the `/info` command in the client to display connection details including TLS status.

### Client (Python)

```bash
cd clients/python
python -m build
python -m pip install .\dist\easy_game_chat-0.3.0-py3-none-any.whl
# or install directly from the source
pip install .
python examples/python/main.py
```

### Token Generation

To generate a token for authentication, use the following command:

```bash
go run main.go generate-token <username>
```

This will generate a token and store its hashed value securely in a `tokens.json` file. The token is valid for 24 hours and can be used for client authentication. Remember to save the token, as it will not be displayed again.

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