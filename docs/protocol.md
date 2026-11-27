# Protocol

EasyGameChat uses TLS over TCP. The default port is 3000. Messages are JSON, and chat messages are newline-delimited after the handshake.

## Handshake

The first client message is:

```json
{"from":"Client","text":"alice","token":"<jwt>"}
```

- `from` is ignored.
- `text` is the requested nickname.
- `token` is required when authentication is enabled. It may be empty in development mode.

The server answers with one JSON object:

```json
{"status":"success"}
```

or:

```json
{"status":"error","message":"Invalid token"}
```

The success response has no trailing newline. Clients must parse the first complete JSON object and preserve any bytes that follow it. The server may send the join notification in the same TCP read.

The initial handshake is read from a 1024-byte buffer.

Authentication and nickname failures close the connection after the error response.

## Chat messages

After the handshake, client messages are:

```json
{"from":"alice","text":"hello"}
```

The server replaces `from` with the authenticated nickname. It broadcasts the message to every other client, not to the sender.

Join and leave notifications use the same shape with `from` set to `Server`:

```json
{"from":"Server","text":"alice joined the chatroom"}
```

Server messages are newline-delimited JSON.

## Limits

| Item | Limit |
|---|---|
| Nickname | 32 characters |
| Message | 512 printable ASCII characters |
| JSON buffer | 4096 bytes |
| Send rate | 10 messages per second per client |
| Connected clients | 100 |
| Token lifetime | 24 hours |

Nicknames must start with a letter or digit and may contain letters, digits, `_`, and `-`. Consecutive `_` or `-` characters are rejected. The names `server`, `admin`, `system`, `null`, and `undefined` are reserved.

Messages must contain at least one non-space character. Control characters and non-ASCII characters are rejected.

## Authentication

Tokens are signed with HS256 and bound to the nickname they were generated for. The binding is case-insensitive.

The token secret must be at least 32 characters. Tokens are not stored by the server and cannot be revoked individually.

## Current limitations

- One global chat room per server
- No message history or offline delivery
- No WebSocket transport
- No protocol version negotiation
- No heartbeat or automatic reconnect
- No message IDs or timestamps
- No private messages or rooms
