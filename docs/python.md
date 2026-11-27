# Python client

The Python client requires Python 3.10 or later.

> The current PyPI release is 0.3.0. The C-style API fixes and the behavior described here are on `main` for 0.4.0.

## Install

```bash
pip install easygamechat
```

From a checkout:

```bash
pip install -e clients/python
```

## Connect

Development server:

```python
from easygamechat import EasyGameChat

client = EasyGameChat("127.0.0.1", 3000, verify_cert=False)
client.connect("alice", "")
```

Production server:

```python
client = EasyGameChat("game.example.com", 3000)
client.connect("alice", token)
```

Certificate verification is enabled by default. Use `verify_cert=False` only for self-signed development certificates. To trust a private CA instead:

```python
client = EasyGameChat("game.example.com", 3000, ca_cert_path="/path/to/ca.pem")
```

The server does not request client certificates, so the client certificate arguments are not used for mutual TLS.

## Messages

```python
def on_message(sender: str, text: str) -> None:
    print(f"[{sender}] {text}")

client.set_message_callback(on_message)
client.send_message("hello")
```

- The callback runs on the receive thread. Keep it short and do not block it.
- `send_message` returns `False` for invalid, rate-limited, or unsent messages.
- Empty or whitespace-only messages are rejected.
- Incoming text is limited to 512 printable ASCII characters.

Connection details:

```python
info = client.get_connection_info()
print(info["connected"], info["host"], info["port"])
```

Disconnect when done:

```python
client.disconnect()
```

## C-style API

The module also exposes a handle-based API:

```python
from easygamechat import (
    egc_connect,
    egc_create_with_certs,
    egc_destroy,
    egc_send,
    egc_set_message_callback,
)

handle = egc_create_with_certs("127.0.0.1", 3000, verify_cert=False)
egc_set_message_callback(handle, lambda sender, text: print(f"[{sender}] {text}"))
egc_connect(handle, "alice", "")
egc_send(handle, "hello")
egc_destroy(handle)
```

`egc_create_with_certs` is the C-style equivalent of passing `verify_cert`, CA, or client certificate options to `EasyGameChat`.
