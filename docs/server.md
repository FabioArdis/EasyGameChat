# Server

The server is a Go binary. It requires Go 1.24 or later.

## Development mode

```bash
go run . --dev
```

Development mode uses `127.0.0.1:3000`, generates a self-signed certificate under `certs/`, and disables authentication. It is intended for local testing only.

You can override the defaults with flags:

```bash
go run . --dev --listen 127.0.0.1:4000
```

If you override `--listen` to a non-loopback address while authentication is disabled, anyone who can reach the port can join.

## Production mode

Production mode requires a TLS certificate, a key, and a JWT secret:

```bash
export EASYGAMECHAT_JWT_SECRET="$(openssl rand -hex 32)"
go run . --listen :3000 --cert /path/to/server.crt --key /path/to/server.key
```

The secret must be at least 32 characters. The server refuses to start if authentication is enabled and the secret is missing or too short.

Generate a token with the same secret:

```bash
go run . generate-token alice
```

Tokens:

- are valid for 24 hours
- are bound to the requested nickname, compared case-insensitively
- are printed once and not stored by the server
- cannot be revoked individually; rotate the secret to invalidate all tokens

## Flags

| Flag | Default | Description |
|---|---|---|
| `--dev` | `false` | Loopback address, generated certificate, authentication disabled |
| `--listen` | `:3000` | Listen address; `127.0.0.1:3000` in development mode |
| `--cert` | `certs/server.crt` | TLS certificate; `certs/dev.crt` in development mode |
| `--key` | `certs/server.key` | TLS private key; `certs/dev.key` in development mode |
| `--log-level` | `info` | `debug`, `info`, `warn`, or `error` |
| `--log-format` | `text` | `text` or `json` |

There is no configuration file yet. Use flags and `EASYGAMECHAT_JWT_SECRET`.

## Limits

| Limit | Value |
|---|---|
| Connected clients | 100 |
| Nickname length | 32 |
| Message length | 512 printable ASCII characters |
| Send rate | 10 messages per second per client |

Nicknames must start with a letter or digit, may contain letters, digits, `_`, and `-`, and may not use consecutive special characters. The names `server`, `admin`, `system`, `null`, and `undefined` are reserved.

## TLS

TLS 1.2 or later is required. Production certificates must be valid for the hostname or IP address clients use to connect.

Development certificates are self-signed. Python and C++ clients verify certificates by default, so they must explicitly opt out for the development server.

## Shutdown and logs

`Ctrl+C` or `SIGTERM` closes the listener and active connections.

Logs go to stderr. Use `--log-level` and `--log-format` to change verbosity and output. Message text and tokens are not logged.
