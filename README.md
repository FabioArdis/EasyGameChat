# EasyGameChat

[![CI](https://github.com/FabioArdis/EasyGameChat/actions/workflows/ci.yml/badge.svg)](https://github.com/FabioArdis/EasyGameChat/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/FabioArdis/EasyGameChat)](https://github.com/FabioArdis/EasyGameChat/releases)
[![PyPI](https://img.shields.io/pypi/v/easygamechat)](https://pypi.org/project/easygamechat/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A small chat server and client library for games and applications. JSON over TLS, a Go server, and Python and C++ clients with no engine dependency.

> Pre-1.0. Current release: 0.4.0.

## Quick start

Start a local server:

```bash
go run . --dev
```

`--dev` listens on `127.0.0.1:3000`, generates a self-signed certificate, and disables authentication.

### Python

```bash
pip install easygamechat
```

```python
from easygamechat import EasyGameChat

client = EasyGameChat("127.0.0.1", 3000, verify_cert=False)
client.set_message_callback(lambda sender, text: print(f"[{sender}] {text}"))
client.connect("alice", "")
client.send_message("hello")
client.disconnect()
```

### C++

```bash
cmake -S clients/cpp -B build
cmake --build build
cmake --install build --prefix /path/to/prefix
```

```cmake
find_package(easygamechat CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE easygamechat::easygamechat)
```

Pass `-DCMAKE_PREFIX_PATH=/path/to/prefix` when configuring your project if the prefix is not on the default search path. See the [C++ client guide](docs/cpp.md) for the full example.

## What it does

- One chat room over TLS
- JWT authentication for production, optional auth in development
- Server-side validation, rate limiting, and connection limits
- Python and C++ clients, plus a C ABI for other engines and bindings
- OpenSSL on Linux/macOS, Schannel on Windows

## Documentation

- [Overview and quickstart](docs/README.md)
- [Server](docs/server.md)
- [Python client](docs/python.md)
- [C++ client](docs/cpp.md)
- [Protocol](docs/protocol.md)
- [Security](docs/security.md)

## Status

Pre-1.0 and under active development. Current limitations: one room, no message history or offline delivery, no WebSocket transport, and no protocol version negotiation. See the [protocol limits](docs/protocol.md#current-limitations).

## License

MIT
