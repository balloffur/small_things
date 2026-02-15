# cyph

------------------------------------------------------------------------

## Overview

cyph is a command-line file encryption tool that produces:

-   `.cyph` encrypted containers (streaming authenticated encryption)
-   `.cyphkey` wrapped key containers
-   Password-based key derivation (Argon2id)
-   Public-key key exchange (`-e`) using Curve25519

The tool uses libsodium for all cryptographic primitives.

------------------------------------------------------------------------

## Cryptographic Primitives

cyph relies exclusively on libsodium:

-   XChaCha20-Poly1305 (secretstream API)
-   Argon2id (`crypto_pwhash`)
-   Curve25519 (ECDH)
-   BLAKE2b (generic hash for fingerprint and shared key derivation)

No custom cryptographic algorithms are implemented.

------------------------------------------------------------------------

## Build Requirements

-   C++17 compatible compiler (g++ / clang / MinGW)
-   libsodium (development package)
-   Standard C++ library

------------------------------------------------------------------------

## Installing libsodium

### Debian / Ubuntu

    sudo apt update
    sudo apt install libsodium-dev

### Windows (MSYS2)

    pacman -S mingw-w64-x86_64-libsodium

------------------------------------------------------------------------

## Compilation

### Linux (libsodium static, libc dynamic)

    g++ -std=c++17 -O2 -Wall -Wextra cyph.cpp -o cyph       -Wl,-Bstatic -lsodium -Wl,-Bdynamic

### Linux (fully static binary)

    g++ -std=c++17 -O2 -Wall -Wextra cyph.cpp -o cyph       -static -static-libgcc -static-libstdc++ -lsodium

### Windows (MinGW fully static)

    x86_64-w64-mingw32-g++ -std=c++17 -O2 -Wall -Wextra cyph.cpp -o cyph.exe       -static -static-libgcc -static-libstdc++ -lsodium

------------------------------------------------------------------------

## Basic Usage

### Encrypt

    cyph -f file.txt -k?

### Decrypt

    cyph -i file.txt.cyph -k?

### Shorthand

    cyph file.txt
    cyph file.txt.cyph
    cyph file.txt key.txt

------------------------------------------------------------------------

## Key Sources

-   -k <file> key from file
-   -k=<text> inline key
-   -k? interactive prompt
-   .cyphkey wrapped key container (-K master key for wrapped key files)

Text keys are normalized: - Lowercase conversion - Whitespace removed

Example: “My Key 123” becomes “mykey123”

------------------------------------------------------------------------

## Wrapped Key Files (.cyphkey)

Create:

    cyph -key rawkey.bin -o wrapped -K?

Use:

    cyph -f file.txt -k wrapped.cyphkey -K?

------------------------------------------------------------------------

## Exchange Mode (-e)

Step 1:

    cyph -e -k? -o mykey

Step 2:

    cyph -e mykey.cyphkey -k?

The shared key replaces the temporary private key.


## KDF Levels

-   -level 0 interactive
-   -level 1 moderate
-   -level 2 sensitive

Parameters are stored in container header.

------------------------------------------------------------------------

## What cyph Protects

-   Confidentiality of file contents
-   Authenticated key exchange

## What It Does Not Protect

-   Metadata

------------------------------------------------------------------------

## Future Plans

-   Native Windows GUI
-   Drag-and-drop encryption
-   Integrated exchange wizard
-   Password strength indicator
-   QR export for public keys

------------------------------------------------------------------------

## License

To be defined.
