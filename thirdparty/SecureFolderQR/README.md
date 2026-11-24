# Secure Folder QR for DoDevEditor

A separate C++17 plugin based on the supplied QR Code Generator. Runs offline, with no shell calls or network service. Includes a standalone recovery CLI and source tests.

## Build in the editor

Copy this folder into the editor's plugin source directory (for example `plugins/SecureFolderQR`). In the parent CMakeLists.txt add:

```cmake
add_subdirectory(plugins/SecureFolderQR)
```

Install OpenSSL development headers and zlib development headers (Arch/CachyOS: `sudo pacman -S openssl zlib`; Debian/Ubuntu: `sudo apt install libssl-dev zlib1g-dev`). Configure and build the parent project as usual. The plugin binary and manifest are emitted to `${PROJECT_BINARY_DIR}/plugins/securefolderqr`. The SDK include root defaults to `${PROJECT_SOURCE_DIR}/include`; override `SFQ_SDK_INCLUDE` if necessary. The SDK header itself was not supplied in QRCode.zip, so the editor module must be compiled against your actual editor SDK; the independent core and CLI have been compiled and tested on Linux.

## Usage

Generate a high-entropy secret file outside the source folder, preferably on a separate protected device:

```sh
umask 077
openssl rand -out "$HOME/folder-qr.key" 32
```

The secret file can alternatively contain a strong random passphrase (at least 16 bytes, maximum 4096; all bytes including trailing newline are significant). Do not send it alongside the QR images. Losing the secret makes the data unrecoverable. Never place the secret inside the source folder. The SDK's supplied example only demonstrates normal text prompts, so the plugin asks for a secret file path, not an exposed password. The password itself is not passed via command arguments or settings.

Open the Secure Folder QR tab or the Tools menu. Select Encrypt, enter the source folder, a **new** output directory with an existing parent, and the secret file path. Output:

- `part-1.png` (and other numbered PNG images)
- `part-1.txt` (and other numbered text payloads)
- `all-parts.txt` (all encrypted text, one part per line)

To recover, scan every image using a QR reader and save the exact text of each in a text file, one per line. Choose Restore, enter the text file path (or original QR export directory), a new destination folder, and the original secret file path. Parts may be supplied in any order. Identical duplicates are accepted within the input size limit; conflicting duplicates, mixed sets and missing parts are rejected. Wrong secrets and altered ciphertext fail authentication before any restored file is written.

## Standalone recovery tool

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/securefolderqr encrypt ./source ./qr-export "$HOME/folder-qr.key"
./build/securefolderqr decrypt ./qr-export/all-parts.txt ./restored "$HOME/folder-qr.key"
```

## Limits and threat model

- 64 MiB maximum serialized uncompressed folder (including headers); up to 256 QR parts, each carrying 1800 Base64 characters plus framing. Maximum compressed bytes 345,548, excluding the 52-byte cryptographic envelope. Large or incompressible folders are rejected rather than truncated. Use conventional encrypted archives for larger data.
- Regular files, nested directories, empty directories and binary content. File timestamps, ACLs, permissions, ownership, hard link relationships and special files are not preserved. Symlinks are rejected. Names that are invalid or reserved on the destination platform may fail restoration. No automatic camera/image decoding is included; use an external QR reader.
- No plaintext archive file is produced; archive is held in memory. This is not hardened against swap, process memory inspection, malicious local users, or concurrent modifications to source/destination directories. Use private trusted directories and protect restored plaintext. The source is not deleted. The key buffers are cleansed on destruction, but full memory erasure is not guaranteed. Operations are synchronous on the calling thread.
- No independent security audit has been performed. Strong random secrets are recommended; PBKDF2 cannot compensate for weak passphrases.

## SFQ1 wire format

All integer fields are big endian. Plain archive begins with ASCII `DIR1`, followed by records: type (ASCII D or F), path byte length (uint32), data byte length (uint32), relative UTF-8 path bytes, then file bytes. Directories have zero data bytes. The entire archive is zlib-compressed. Envelope: ASCII `SFQ1` (4), random salt (16), random GCM nonce (12), uncompressed size (4), ciphertext (variable), authentication tag (16). The first 36 bytes are authenticated as GCM AAD. Key: PBKDF2-HMAC-SHA256, 600000 iterations, 32 bytes, secret file exact bytes. AES-256-GCM uses a 12-byte nonce and 16-byte tag. Standard padded Base64 of entire envelope is split into 1800-character chunks. Each text line is `SFQ1:<id>:<index>:<total>:<chunk>`; id is standard Base64 of the first 12 bytes of the salt, indices are 1-based. QR uses medium error correction, byte mode, PNG scale 4. The set id is only for assembly, not authentication.

## Tests

Core CTest covers round trip, multiple parts, binary data, nested/empty directories, reordering, missing data, ciphertext tampering and incorrect secrets. Editor UI and real camera scanning have not been tested. QRMatrix is reused from the supplied archive; see its license.
