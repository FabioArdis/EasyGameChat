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

1. Install Go 1.24 or later.
2. Run the server locally:

```bash
go run . --dev
```

`--dev` binds to `127.0.0.1:3000`, generates a development certificate, and disables authentication.

For production, provide a TLS certificate with `--cert` and `--key`, and set `EASYGAMECHAT_JWT_SECRET` to at least 32 characters.

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

Certificate verification is enabled by default. The example disables it for the local self-signed development server with `egc_set_tls_verification`. Do not disable it in production.

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

Set a secret of at least 32 characters and keep it for the server and token generation:

```bash
export EASYGAMECHAT_JWT_SECRET="$(openssl rand -hex 32)"
go run . generate-token <username>
```

The token is valid for 24 hours, bound to the username, and not stored by the server. Save it when it is displayed.

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