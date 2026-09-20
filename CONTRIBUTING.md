# Contributing

Thanks for helping make multipassword better.

## Ground rules

- **Security first.** Anything touching `src/core` (crypto, vault format, memory handling) needs a test in `tests/test_main.cpp` and a short note in the PR explaining the threat model impact. Never add a way to export or log plaintext secrets.
- **No new crypto.** Use libsodium primitives only; do not implement ciphers, hashes or KDFs by hand.
- **Vault format changes** must bump the format version and remain readable by `VaultFormat::parseHeader` for older files.
- Keep the UI faithful to the reference design: dark three-pane layout, colours from `src/ui/Theme.h`.

## Workflow

1. Fork and branch from `main`.
2. `.\build.ps1 test` must pass (all checks, zero failures).
3. Format with `clang-format` (config in `.clang-format`).
4. Open a PR; CI builds on Windows and runs the tests.

## Reporting security issues

Please use GitHub's private vulnerability reporting (Security tab → "Report a vulnerability") instead of a public issue. See `SECURITY.md`.
