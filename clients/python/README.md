# easygamechat

[![PyPI](https://img.shields.io/pypi/v/easygamechat)](https://pypi.org/project/easygamechat/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/license/mit/)

Python client for EasyGameChat, a small chat server for games and applications. Requires Python 3.10 or later.

## Install

```bash
pip install easygamechat
```

## Quick start

Start a development server from the repository:

```bash
go run . --dev
```

Then connect:

```python
from easygamechat import EasyGameChat

client = EasyGameChat("127.0.0.1", 3000, verify_cert=False)
client.set_message_callback(lambda sender, text: print(f"[{sender}] {text}"))
client.connect("alice", "")
client.send_message("hello")
client.disconnect()
```

`verify_cert=False` is only for the self-signed development certificate. Certificate verification is enabled by default.

For a production server:

```python
client = EasyGameChat("game.example.com", 3000)
client.connect("alice", token)
```

## Documentation

- [Python client guide](https://github.com/FabioArdis/EasyGameChat/blob/main/docs/python.md)
- [Server setup](https://github.com/FabioArdis/EasyGameChat/blob/main/docs/server.md)
- [Protocol](https://github.com/FabioArdis/EasyGameChat/blob/main/docs/protocol.md)
- [Security](https://github.com/FabioArdis/EasyGameChat/blob/main/docs/security.md)

## License

MIT
