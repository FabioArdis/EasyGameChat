# Security

EasyGameChat is pre-1.0 and has not undergone an independent security audit. Treat it accordingly when deploying it publicly.

## Transport

- TLS 1.2 or later is required.
- Python and C++ clients verify the server certificate and hostname by default.
- Development mode uses a self-signed certificate and disables authentication. Do not expose it.
- Clients must opt out of verification explicitly for the development certificate:
  - Python: `verify_cert=False`
  - C++: `setTLSVerification(false)` or `TLSConfig{false}`
  - C: `egc_set_tls_verification(client, false)`

## Tokens

- Tokens are JWTs signed with HS256.
- Tokens expire after 24 hours.
- Tokens are bound to the nickname they were generated for.
- The server requires `EASYGAMECHAT_JWT_SECRET` to be at least 32 characters.
- Tokens are not stored by the server.
- Individual tokens cannot be revoked. Rotating the secret invalidates every existing token.

Do not commit the secret, certificates, private keys, or generated tokens to the repository.

## Server behavior

- Nicknames and messages are validated server-side.
- Messages are limited to 512 printable ASCII characters.
- Clients are rate-limited to 10 messages per second.
- The server accepts at most 100 concurrent clients.
- Message history is not stored; the server keeps the current room in memory only.

The current server logs connection and message activity to stdout, including message text. Do not treat server logs as confidential.

## Client behavior

- C++ uses the operating system certificate store. A custom CA file is not supported yet.
- Python accepts `ca_cert_path` for a private CA.
- The server does not request client certificates, so mutual TLS is not available.
- Message callbacks run on the network thread. Keep them short and do not block.

## Reporting

Report security issues through GitHub's private vulnerability reporting:

https://github.com/FabioArdis/EasyGameChat/security/advisories/new

Do not open a public issue for a vulnerability.
