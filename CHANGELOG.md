# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog, and this project adheres to Semantic Versioning.

## [Unreleased]

## [0.4.0] - 2026-11-27

### Added

- Local development mode with `--dev`, generated TLS certificates, and authentication disabled.
- Structured server logging with `--log-level` and `--log-format`.
- C++ static library with a CMake package and a C ABI.
- C++ TLS verification controls with verification enabled by default.
- DNS, IPv4, and IPv6 support in the C++ client.
- Dockerfile and tag-triggered release workflows for PyPI, server binaries, C++ sources, and GHCR.
- Documentation and GitBook sync configuration.
- CI for Go, Python, and C++ on Linux, plus installed-package consumer builds.

### Changed

- JWT authentication now requires `EASYGAMECHAT_JWT_SECRET` with at least 32 characters.
- Tokens require an expiration and use HS256 only.
- Tokens are bound to the nickname they were generated for.
- The C++ client is built as `easygamechat::easygamechat`; the public headers no longer expose OpenSSL or nlohmann/json.
- The Python client requires Python 3.10 or later.
- The Python receive path preserves messages that follow the authentication response.
- The Go module path is now `github.com/FabioArdis/EasyGameChat`.
- Server logs omit message text and tokens.

### Fixed

- C++ client builds against OpenSSL 4.
- C++ receive loop no longer reads an uninitialized value.
- C++ client resolves hostnames and accepts IPv4/IPv6 addresses.
- Python `TLSError`, `egc_connect`, and `egc_create_with_certs` exports.
- Python clients no longer drop valid 512-character messages.

### Removed

- Hardcoded JWT secret.
- `tokens.json` token storage.
- The old single-header `clients/c-cpp/EasyGameChat.h` client.
- Raw payload logging and unused debug helpers.

### Security

- TLS certificate verification is enabled by default in the C++ client.
- Tokens are bound to nicknames and cannot be used across users.
- Authentication failures do not log token values.
- Development mode is loopback-only unless the listen address is overridden.

### Breaking changes

- Existing tokens signed with the old hardcoded secret are invalid.
- Production servers must set `EASYGAMECHAT_JWT_SECRET`.
- The Go module path changed to `github.com/FabioArdis/EasyGameChat`.
- C++ users must link the CMake target and explicitly disable verification for self-signed development certificates.
- Python 3.7, 3.8, and 3.9 are no longer supported.

## [0.3.0] - 2025-06-15

### Added

- Token authentication for the Go server, Python client, and C++ client.
- `generate-token` command.

## [0.2.0] - 2025-06-07

### Added

- TLS encryption.
- Connection information and `/info` commands.
- Graceful shutdown.

## [0.1.0] - 2025-06-03

### Added

- Initial Go server with Python and C++ clients.
- JSON message protocol, validation, rate limiting, and broadcasting.

[Unreleased]: https://github.com/FabioArdis/EasyGameChat/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/FabioArdis/EasyGameChat/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/FabioArdis/EasyGameChat/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/FabioArdis/EasyGameChat/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/FabioArdis/EasyGameChat/releases/tag/v0.1.0
