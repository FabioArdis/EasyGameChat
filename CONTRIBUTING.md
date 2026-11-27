# Contributing

EasyGameChat is pre-1.0. Small, focused changes are easier to review and merge.

## Before you start

- Open an issue for bugs and feature requests.
- Report security issues privately as described in [SECURITY.md](SECURITY.md). Do not open a public issue.
- Keep changes focused. Avoid unrelated formatting or refactors.

## Development setup

Requirements: Go 1.24 or later, Python 3.10 or later, CMake 3.15 or later, and a C++14 compiler.

### Server

```bash
go test ./...
go run . --dev
```

### Python client

```bash
cd clients/python
python3 -m venv .venv
.venv/bin/pip install -e ".[dev]"
.venv/bin/pytest
```

### C++ client

The library requires `nlohmann/json`. OpenSSL is required on Linux and macOS. Windows uses Schannel.

```bash
cmake -S clients/cpp -B build -DEASYGAMECHAT_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Branches and commits

- Branch from `main` using `feat/`, `fix/`, `docs/`, `ci/`, `test/`, `refactor/`, or `chore/`.
- Use Conventional Commits, for example `fix(python): preserve buffered messages`.
- Keep each pull request to one concern and link the related issue.
- Open a draft pull request early if you want feedback.

## Before opening a pull request

- Run the tests for the components you changed.
- Run `gofmt` for Go changes.
- Update documentation and [CHANGELOG.md](CHANGELOG.md) for user-visible changes.
- Remove tokens, secrets, certificates, and private data from logs and examples.

## Pull requests

Pull requests are squash-merged after CI passes. Keep the title in Conventional Commit form and explain why the change is needed, not only what changed.

## License

By contributing, you agree that your contribution is licensed under the MIT License.
