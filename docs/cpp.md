# C++ client

The C++ client is a static library with a thin public header. It requires CMake 3.15 or later and a C++14 compiler.

Build dependencies:

- `nlohmann/json` at build time only
- OpenSSL on Linux and macOS
- Schannel on Windows, with no OpenSSL dependency

The public headers do not include OpenSSL or `nlohmann/json`.

## Build and install

```bash
cmake -S clients/cpp -B build
cmake --build build
cmake --install build --prefix /path/to/prefix
```

In a consumer project:

```cmake
find_package(easygamechat CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE easygamechat::easygamechat)
```

Pass the install prefix with `-DCMAKE_PREFIX_PATH=/path/to/prefix` when it is not a standard location.

## C++ API

```cpp
#include <easygamechat/easygamechat.hpp>
#include <iostream>

egc::EasyGameChat client("127.0.0.1", 3000);
client.setTLSVerification(false); // development only

client.setMessageCallback([](const std::string& sender, const std::string& text) {
  std::cout << "[" << sender << "] " << text << "\n";
});

if (!client.connect("alice", "")) {
  return 1;
}

client.sendMessage("hello");
```

`setTLSVerification` must be called before `connect`. Certificate verification is enabled by default.

Configuration can also be passed to the constructor:

```cpp
egc::TLSConfig tls;
tls.verifyCertificate = false;

egc::EasyGameChat client("127.0.0.1", 3000, true, tls);
```

Production connections use the system certificate store. A custom CA file is not supported by the C++ client yet; install the CA in the operating system store.

The client accepts DNS names and IPv4 or IPv6 literals.

## Connection information

```cpp
auto info = client.getConnectionInfo();
std::cout << info.tls_version << " " << info.tls_cipher << "\n";
std::cout << "verified: " << info.server_cert_valid << "\n";
```

`server_cert_valid` is true only when certificate verification succeeded.

## C API

The C ABI is available from `<easygamechat/easygamechat.h>` and is intended for bindings and engines without a C++ interface.

```c
#include <easygamechat/easygamechat.h>
#include <stdio.h>

static void on_message(const char* from, const char* text, void* user_data) {
  (void)user_data;
  printf("[%s] %s\n", from, text);
}

egc_client_t* client = egc_create("127.0.0.1", 3000);
if (!client) {
  return 1;
}

egc_set_tls_verification(client, false); // development only
egc_set_message_callback(client, on_message, NULL);

if (!egc_connect(client, "alice", "")) {
  egc_destroy(client);
  return 1;
}

egc_send(client, "hello");
egc_destroy(client);
```

The callback runs on the receive thread. Copy any strings you need to keep.

## Example

The complete example is in `examples/c-cpp/main.cpp`.

```bash
cd examples/c-cpp
cmake -B build -S .
cmake --build build
./build/chat_client
```
