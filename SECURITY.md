# Security design

## Threat model

**Protects against**
- Theft of the vault file (disk, backup, cloud sync): contents are encrypted and authenticated; offline guessing is slowed by Argon2id (256 MiB, 3 passes by default).
- Tampering with the vault file: every byte of header and body is covered by Poly1305 authentication; any modification is rejected before parsing.
- Casual memory disclosure (swap file, hibernation, crash dumps): secrets are kept in `sodium_malloc` memory (guard pages, `mlock`, canaries) and wiped on release; WER dumps are disabled.
- Shoulder surfing / screen sharing: secrets are masked by default; optional exclusion from screen capture.
- Clipboard leaks: sensitive copies auto-clear and opt out of Windows clipboard history and cloud clipboard.
- DLL preloading: the process only loads DLLs from the application and system directories.
- Typing a secret into the wrong window: autofill verifies the foreground window before every keystroke.

**Does not protect against**
- Malware running as your user while the vault is unlocked (it can read process memory or record keystrokes).
- A weak master password. There is no recovery: the KDF output is the only way to unwrap the vault key.
- Hardware attacks / cold boot.

## Cryptography

| Purpose | Algorithm | Parameters |
|---|---|---|
| Key derivation | Argon2id v1.3 (`crypto_pwhash`) | 256 MiB, 3 passes (stored in header, bounded on parse) |
| Key wrapping & body | XChaCha20-Poly1305-IETF | 256-bit key, 192-bit random nonce, 128-bit tag |
| Random | `randombytes_buf` / `randombytes_uniform` | OS CSPRNG, no modulo bias |
| BIP-39 checksum | SHA-256 | |
| TOTP | HMAC-SHA1 (RFC 6238) | SHA-1 is used only where the standard mandates it |

The master password is converted to `SecureBytes` at the UI boundary, used once for derivation, then wiped. Changing the master password re-wraps the vault key under a fresh salt and key; the body is not re-encrypted (its key did not change).

## Known limitations

- Qt string types (`QString`, `QLineEdit`) hold plaintext while an item is displayed or edited. We wipe those buffers where we own them, but Qt may keep transient copies (undo stacks, layout caches). A locked vault holds none of them.
- The BIP-39 wordlist is English only.
- Autofill is implemented for Windows; other platforms compile with autofill disabled.

## Reporting

Please open a private security advisory on GitHub rather than a public issue.
