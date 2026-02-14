// cyph.cpp
// Build (recommended: static libsodium only):
//   g++ -std=c++17 -O2 -Wall -Wextra cyph.cpp -o cyph \
//     -Wl,-Bstatic -lsodium -Wl,-Bdynamic
//
// Build (fully static; may require libc static packages and may fail on some distros):
//   g++ -std=c++17 -O2 -Wall -Wextra cyph.cpp -o cyph -static -lsodium
//
// Install libsodium (Debian/Ubuntu):
//   sudo apt update
//   sudo apt install libsodium-dev
//
// Usage:
//   Encrypt: cyph -f input.txt -k keys.txt -o out.mycyph [-WIPE]
//   Decrypt: cyph -f out.mycyph -k keys.txt [-o out] [-s] [-WIPE]
//   Keyfile mode:
//     - if keyfile ends with .txt => TEXT mode: remove whitespace + lowercase
//     - otherwise => BINARY mode: raw bytes (no transforms)

#include <sodium.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


static bool has_ext(const std::string& path, const std::string& ext) {
    if (path.size() < ext.size()) return false;
    return path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
}

// "Has any extension" = last '.' after last path separator
static bool has_any_extension(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    const size_t dot = path.find_last_of('.');
    return (dot != std::string::npos) && (slash == std::string::npos || dot > slash);
}

static std::vector<unsigned char> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file for reading: " + path);

    f.seekg(0, std::ios::end);
    std::streamoff n = f.tellg();
    f.seekg(0, std::ios::beg);
    if (n < 0) throw std::runtime_error("Failed to stat file: " + path);

    std::vector<unsigned char> buf(static_cast<size_t>(n));
    if (n > 0) {
        f.read(reinterpret_cast<char*>(buf.data()), n);
        if (!f) throw std::runtime_error("Failed to read file: " + path);
    }
    return buf;
}

static void write_file(const std::string& path, const std::vector<unsigned char>& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("Cannot open file for writing: " + path);

    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    if (!f) throw std::runtime_error("Failed to write file: " + path);
}

static void write_stdout(const std::vector<unsigned char>& data) {
    std::cout.write(reinterpret_cast<const char*>(data.data()),
                    static_cast<std::streamsize>(data.size()));
    if (!std::cout) throw std::runtime_error("Failed to write to stdout");
}

static bool confirm_delete(const std::vector<std::string>& paths) {
    std::cerr << "WARNING: You requested deletion of the following file(s):\n";
    for (const auto& p : paths) std::cerr << "  - " << p << "\n";
    std::cerr << "This operation is permanent. Type YES to confirm: ";

    std::string ans;
    std::getline(std::cin, ans);
    return ans == "YES";
}

static void delete_file_or_throw(const std::string& path) {
    if (std::remove(path.c_str()) != 0) {
        throw std::runtime_error("Failed to delete file: " + path);
    }
}

// Key material:
//  - if .txt => normalize: remove whitespace + lowercase
//  - else => raw bytes
static std::vector<unsigned char> read_key_material(const std::string& key_path) {
    auto data = read_file(key_path);

    if (has_ext(key_path, ".txt")) {
        std::string text(reinterpret_cast<const char*>(data.data()), data.size());
        std::string normalized;
        normalized.reserve(text.size());

        for (unsigned char ch : text) {
            if (std::isspace(ch)) continue;
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }

        // Wipe temporary text buffers best-effort
        if (!text.empty()) sodium_memzero(text.data(), text.size());

        std::vector<unsigned char> out(normalized.begin(), normalized.end());

        if (!normalized.empty()) sodium_memzero(normalized.data(), normalized.size());
        return out;
    }

    // Binary mode: raw bytes untouched
    return data;
}

static void secure_wipe(std::vector<unsigned char>& v) {
    if (!v.empty()) sodium_memzero(v.data(), v.size());
    v.clear();
    v.shrink_to_fit(); // best-effort
}

//format

static const unsigned char MAGIC[8] = {'M','Y','C','Y','P','H','1',0};

static constexpr size_t SALT_LEN  = crypto_pwhash_SALTBYTES; // 16
static constexpr size_t NONCE_LEN = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES; // 24
static constexpr size_t KEY_LEN   = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;  // 32
static constexpr size_t ABYTES    = crypto_aead_xchacha20poly1305_ietf_ABYTES;    // 16


static constexpr unsigned long long OPSLIMIT = crypto_pwhash_OPSLIMIT_SENSITIVE;
static constexpr size_t MEMLIMIT = crypto_pwhash_MEMLIMIT_SENSITIVE;

static void derive_key(unsigned char key[KEY_LEN],
                       const unsigned char salt[SALT_LEN],
                       const std::vector<unsigned char>& key_material) {
    if (key_material.empty()) {
        throw std::runtime_error("Key material is empty");
    }

    // libsodium expects password as char*, but it's just bytes + length.
    const char* pw = reinterpret_cast<const char*>(key_material.data());
    const size_t pwlen = key_material.size();

    if (crypto_pwhash(
            key, KEY_LEN,
            pw, pwlen,
            salt,
            OPSLIMIT, MEMLIMIT,
            crypto_pwhash_ALG_DEFAULT
        ) != 0) {
        throw std::runtime_error("crypto_pwhash failed (out of memory?)");
    }
}

static std::vector<unsigned char> encrypt_blob(
    const std::vector<unsigned char>& plaintext,
    const std::vector<unsigned char>& key_material
) {
    unsigned char salt[SALT_LEN];
    unsigned char nonce[NONCE_LEN];
    unsigned char key[KEY_LEN];

    randombytes_buf(salt, sizeof salt);
    randombytes_buf(nonce, sizeof nonce);

    derive_key(key, salt, key_material);

    std::vector<unsigned char> ciphertext(plaintext.size() + ABYTES);
    unsigned long long clen = 0;

    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            ciphertext.data(), &clen,
            plaintext.data(), plaintext.size(),
            nullptr, 0,   // AAD (none)
            nullptr,
            nonce, key
        ) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Encryption failed");
    }
    ciphertext.resize(static_cast<size_t>(clen));

    sodium_memzero(key, sizeof key);

    std::vector<unsigned char> out;
    out.reserve(sizeof(MAGIC) + SALT_LEN + NONCE_LEN + ciphertext.size());
    out.insert(out.end(), MAGIC, MAGIC + sizeof(MAGIC));
    out.insert(out.end(), salt, salt + sizeof(salt));
    out.insert(out.end(), nonce, nonce + sizeof(nonce));
    out.insert(out.end(), ciphertext.begin(), ciphertext.end());
    return out;
}

static std::vector<unsigned char> decrypt_blob(
    const std::vector<unsigned char>& blob,
    const std::vector<unsigned char>& key_material
) {
    const size_t header = sizeof(MAGIC) + SALT_LEN + NONCE_LEN;
    if (blob.size() < header + ABYTES) {
        throw std::runtime_error("Invalid .mycyph file (too short)");
    }
    if (sodium_memcmp(blob.data(), MAGIC, sizeof(MAGIC)) != 0) {
        throw std::runtime_error("Invalid .mycyph file (bad magic/version)");
    }

    const unsigned char* salt  = blob.data() + sizeof(MAGIC);
    const unsigned char* nonce = blob.data() + sizeof(MAGIC) + SALT_LEN;
    const unsigned char* c     = blob.data() + header;
    const size_t clen          = blob.size() - header;

    if (clen < ABYTES) {
        throw std::runtime_error("Corrupted .mycyph file (ciphertext too short)");
    }

    unsigned char key[KEY_LEN];
    derive_key(key, salt, key_material);

    std::vector<unsigned char> plaintext(clen - ABYTES);
    unsigned long long plen = 0;

    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            plaintext.data(), &plen,
            nullptr,
            c, clen,
            nullptr, 0,
            nonce, key
        ) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Wrong key material or corrupted file");
    }

    sodium_memzero(key, sizeof key);
    plaintext.resize(static_cast<size_t>(plen));
    return plaintext;
}

// ------------------------- CLI parsing -------------------------

struct Args {
    std::string input;   // -f
    std::string keyfile; // -k
    std::string output;  // -o (optional)
    bool show_stdout = false; // -s
    bool wipe = false;        // -WIPE
};

static void usage() {
    std::cerr <<
R"(cyph - passphrase/file-based encryption tool (.mycyph)

Usage:
  cyph -f <file> -k <keyfile> [-o <output>] [-s] [-WIPE]

Mode selection:
  If -f does NOT end with .mycyph  -> encrypt (requires -o ending with .mycyph)
  If -f ends with .mycyph          -> decrypt

Options:
  -f <file>        Input file
  -k <keyfile>     Key file:
                   - if ends with .txt => TEXT mode (whitespace removed, lowercased)
                   - otherwise         => BINARY mode (raw bytes)
  -o <output>      Output file
                   Decrypt: if <output> has no extension => default .txt
  -s               Show decrypted data on stdout (best for text)
  -WIPE            Ask for confirmation, then delete files:
                   Encrypt: delete input (-f) and keyfile (-k) after success
                   Decrypt: delete keyfile (-k) after success

Examples:
  cyph -f note.txt   -k keys.txt   -o note.mycyph
  cyph -f note.mycyph -k keys.txt  -s
  cyph -f note.mycyph -k key.bin   -o out -s
  cyph -f note.txt   -k keys.txt   -o note.mycyph -WIPE
)";
}

static Args parse_args(int argc, char** argv) {
    Args a;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        auto need = [&](const std::string& opt) {
            if (i + 1 >= argc) throw std::runtime_error("Missing value for " + opt);
            return std::string(argv[++i]);
        };

        if (arg == "-f") {
            a.input = need("-f");
        } else if (arg == "-k") {
            a.keyfile = need("-k");
        } else if (arg == "-o") {
            a.output = need("-o");
        } else if (arg == "-s") {
            a.show_stdout = true;
        } else if (arg == "-WIPE") {
            a.wipe = true;
        } else if (arg == "-h" || arg == "--help") {
            usage();
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (a.input.empty() || a.keyfile.empty()) {
        throw std::runtime_error("Both -f and -k are required");
    }

    return a;
}

int main(int argc, char** argv) {
    if (sodium_init() < 0) {
        std::cerr << "sodium_init failed\n";
        return 1;
    }

    try {
        Args args = parse_args(argc, argv);
        const bool decrypt_mode = has_ext(args.input, ".mycyph");

        // Read key material (text-normalized if .txt, raw otherwise)
        std::vector<unsigned char> key_material = read_key_material(args.keyfile);
        if (key_material.empty()) {
            throw std::runtime_error("Key material is empty (after normalization, if .txt)");
        }

        if (!decrypt_mode) {
            // Encrypt
            if (args.output.empty()) {
                throw std::runtime_error("Encryption requires -o <output.mycyph>");
            }
            if (!has_ext(args.output, ".mycyph")) {
                throw std::runtime_error("Encrypted output must end with .mycyph");
            }

            std::vector<unsigned char> plaintext = read_file(args.input);
            std::vector<unsigned char> blob = encrypt_blob(plaintext, key_material);
            write_file(args.output, blob);

            // Best-effort wipe plaintext buffer now that it's encrypted
            secure_wipe(plaintext);

            std::cout << "Encrypted: " << args.output << "\n";

            if (args.wipe) {
                std::vector<std::string> targets = {args.input, args.keyfile};
                if (!confirm_delete(targets)) {
                    std::cout << "Deletion cancelled.\n";
                    secure_wipe(key_material);
                    return 0;
                }
                delete_file_or_throw(args.input);
                delete_file_or_throw(args.keyfile);
                std::cout << "Deleted input and key file.\n";
            }

        } else {
            // decrypt
            std::vector<unsigned char> blob = read_file(args.input);
            std::vector<unsigned char> plaintext = decrypt_blob(blob, key_material);

            // Show stdout (optional)
            if (args.show_stdout) {
                write_stdout(plaintext);
            }

            // Save to file (optional)
            if (!args.output.empty()) {
                std::string out = args.output;
                if (!has_any_extension(out)) out += ".txt";
                write_file(out, plaintext);
                std::cout << "\nDecrypted saved to: " << out << "\n";
            }

            if (!args.show_stdout && args.output.empty()) {
                std::cout << "Decryption successful (no output written).\n";
            }

            // Best-effort wipe plaintext once we're done outputting
            secure_wipe(plaintext);

            if (args.wipe) {
                std::vector<std::string> targets = {args.keyfile};
                if (!confirm_delete(targets)) {
                    std::cout << "Deletion cancelled.\n";
                    secure_wipe(key_material);
                    return 0;
                }
                delete_file_or_throw(args.keyfile);
                std::cout << "Deleted key file.\n";
            }
        }

        // Best-effort wipe key material at end
        secure_wipe(key_material);
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        usage();
        return 2;
    }
}
