# EasyGameChat documentation

EasyGameChat is a small chat server and client library for games and applications. The server is written in Go. Python and C++ clients are included, and the C ABI can be used from other engines and languages.

The protocol is JSON over TLS, with one global chat room per server.

> These docs track the `main` branch. The project is pre-1.0.

## 5-minute quickstart

### 1. Start the server

```bash
go run . --dev
```

`--dev` does three things:

- binds to `127.0.0.1:3000`
- generates `certs/dev.crt` and `certs/dev.key` if needed
- disables authentication

Do not expose development mode to untrusted networks.

### 2. Connect from Python

```bash
pip install easygamechat
```

```python
from easygamechat import EasyGameChat

client = EasyGameChat("127.0.0.1", 3000, verify_cert=False)
client.set_message_callback(lambda sender, text: print(f"[{sender}] {text}"))
client.connect("alice", "")
client.send_message("hello")
```

Keep the process running to receive messages. Call `client.disconnect()` when done.

### 3. Connect from C++

```bash
cmake -S clients/cpp -B build
cmake --build build
cmake --install build --prefix /path/to/prefix
```

```cmake
find_package(easygamechat CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE easygamechat::easygamechat)
```

```cpp
#include <easygamechat/easygamechat.hpp>
#include <iostream>

egc::EasyGameChat client("127.0.0.1", 3000);
client.setTLSVerification(false);
client.setMessageCallback([](const std::string& sender, const std::string& text) {
  std::cout << "[" << sender << "] " << text << "\n";
});
client.connect("bob", "");
client.sendMessage("hello from C++");
```

See the [C++ client guide](cpp.md) for the full example.

## Production setup

Production mode requires TLS certificates and a JWT secret:

```bash
export EASYGAMECHAT_JWT_SECRET="$(openssl rand -hex 32)"
go run . --listen :3000 --cert /path/to/server.crt --key /path/to/server.key
```

Generate tokens for your users with the same secret:

```bash
go run . generate-token alice
```

See [Server](server.md) and [Security](security.md) for details.

## Guides

- [Server](server.md)
- [Python client](python.md)
- [C++ client](cpp.md)
- [Protocol](protocol.md)
- [Security](security.md)
