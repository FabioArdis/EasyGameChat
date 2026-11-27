# Security Policy

EasyGameChat is pre-1.0. Security fixes are provided for the latest release only.

| Version | Supported |
|---|---|
| 0.4.x | Yes |
| Earlier | No |

## Reporting a vulnerability

Do not open a public issue. Use GitHub private vulnerability reporting:

https://github.com/FabioArdis/EasyGameChat/security/advisories/new

Include:

- affected version or commit
- affected component
- steps to reproduce
- expected and actual impact
- any suggested fix or mitigation

Reports are handled on a best-effort basis. There is no paid bug bounty and no guaranteed response time.

## Scope

This policy covers the Go server, the Python and C++ clients, the wire protocol, and release artifacts.

Development mode (`--dev`) disables authentication and uses a self-signed certificate. It is intended for loopback use only.
