/**
 * @file cyph.cpp
 * @brief Streaming file encryption tool producing .cyph containers and .cyphkey wrapped key containers.
 *
 * Build (dynamic libc, static libsodium):
 *   g++ -std=c++17 -O2 -Wall -Wextra cyph.cpp -o cyph -Wl,-Bstatic -lsodium -Wl,-Bdynamic
 *
 * Build (fully static; may require static libc toolchain/packages):
 *   g++ -std=c++17 -O2 -Wall -Wextra cyph.cpp -o cyph -static -static-libgcc -static-libstdc++ -lsodium
 */

#include <sodium.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static constexpr const char* VERSION = "2.3.2";

/** @brief Encrypted container extension. */
static constexpr const char* EXT_CYPH = ".cyph";
/** @brief Wrapped key container extension. */
static constexpr const char* EXT_CYPHKEY = ".cyphkey";
/** @brief Output file for -manprint. */
static constexpr const char* MANUAL_FILE = "cyph_manual.txt";

/* ============================
 * Utility helpers
 * ============================ */

/**
 * @brief Check whether a path ends with a given extension.
 * @param path Input path string.
 * @param ext Extension including dot (e.g., ".cyph").
 * @return True if path ends with ext.
 */
static bool has_ext(const std::string& path, const std::string& ext) {
    if (path.size() < ext.size()) return false;
    return path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
}

/**
 * @brief Return true if argument looks like an option (starts with '-').
 */
static bool looks_like_option(const std::string& s) {
    return !s.empty() && s[0] == '-';
}

/**
 * @brief Normalize text: lowercase and remove all whitespace.
 */
static std::string lowercase_no_ws(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char ch : s) {
        if (std::isspace(ch)) continue;
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

/**
 * @brief Read an entire file into memory (binary).
 * @throws std::runtime_error on failure.
 */
static std::vector<unsigned char> read_file_bytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open for reading: " + path);

    f.seekg(0, std::ios::end);
    std::streamoff n = f.tellg();
    f.seekg(0, std::ios::beg);
    if (n < 0) throw std::runtime_error("Failed to stat: " + path);

    std::vector<unsigned char> buf(static_cast<size_t>(n));
    if (n > 0) {
        f.read(reinterpret_cast<char*>(buf.data()), n);
        if (!f) throw std::runtime_error("Failed to read: " + path);
    }
    return buf;
}

/**
 * @brief Write bytes to a file, creating parent directories as needed.
 * @throws std::runtime_error on failure.
 */
static void write_file_bytes(const std::string& path, const std::vector<unsigned char>& data) {
    fs::path p(path);
    fs::path parent = p.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) throw std::runtime_error("Failed to create directory: " + parent.string());
    }

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("Cannot open for writing: " + path);

    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    if (!f) throw std::runtime_error("Failed to write: " + path);
}

/**
 * @brief Write bytes to stdout.
 * @throws std::runtime_error on failure.
 */
static void write_stdout(const unsigned char* p, size_t n) {
    std::cout.write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(n));
    if (!std::cout) throw std::runtime_error("Failed to write to stdout");
}

/**
 * @brief Best-effort secure wipe of vector contents.
 */
static void secure_wipe(std::vector<unsigned char>& v) {
    if (!v.empty()) sodium_memzero(v.data(), v.size());
    v.clear();
    v.shrink_to_fit();
}

/**
 * @brief Best-effort remove file.
 * @return True if removed.
 */
static bool remove_file_best_effort(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

/**
 * @brief Ask user to confirm destructive deletion.
 */
static bool confirm_wipe(const std::vector<std::string>& targets) {
    std::cerr << "WARNING: You requested -WIPE. After SUCCESS, the following file(s) will be deleted:\n";
    for (const auto& t : targets) std::cerr << "  - " << t << "\n";
    std::cerr << "This operation is permanent. Confirm? (Yes/No): ";

    std::string ans;
    std::getline(std::cin, ans);

    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!ans.empty() && is_space(static_cast<unsigned char>(ans.front()))) ans.erase(ans.begin());
    while (!ans.empty() && is_space(static_cast<unsigned char>(ans.back()))) ans.pop_back();
    for (auto& c : ans) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    return (ans == "yes" || ans == "y");
}

/**
 * @brief Ensure a filename ends with the given extension.
 */
static std::string ensure_ext(std::string out, const std::string& ext) {
    if (!has_ext(out, ext)) out += ext;
    return out;
}

static std::string ensure_cyph_ext(std::string out) { return ensure_ext(std::move(out), EXT_CYPH); }
static std::string ensure_cyphkey_ext(std::string out) { return ensure_ext(std::move(out), EXT_CYPHKEY); }

/**
 * @brief Return basename of a path.
 */
static std::string basename_only(const std::string& p) {
    return fs::path(p).filename().string();
}

/**
 * @brief Ensure directory exists.
 * @throws std::runtime_error on failure.
 */
static void ensure_dir_exists(const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) throw std::runtime_error("Failed to create directory: " + dir.string());
}

/**
 * @brief Prompt user for a line of text.
 */
static std::string prompt_line(const std::string& prompt) {
    std::cerr << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

/**
 * @brief Prompt for a key, normalize it (lowercase + remove whitespace) and return bytes.
 */
static std::vector<unsigned char> prompt_key_normalized(const std::string& prompt) {
    std::string s = prompt_line(prompt);
    std::string norm = lowercase_no_ws(s);
    if (!s.empty()) sodium_memzero(s.data(), s.size());
    std::vector<unsigned char> out(norm.begin(), norm.end());
    if (!norm.empty()) sodium_memzero(norm.data(), norm.size());
    return out;
}

/* ============================
 * Manual text
 * ============================ */

/**
 * @brief Full manual/tutorial text.
 */
static const char* MANUAL_TEXT =
R"(cyph manual (tutorial + internals)
================================

0) What cyph is
---------------
cyph encrypts files into a single container with extension ".cyph".
It also supports wrapped key files ".cyphkey" (an encrypted container that stores key material).

1) Quick start
--------------
Encrypt a file (prompt for key):
  cyph secret.txt
  (will ask "Enter key (-k?):", then write "secret.txt.cyph" unless you specify -o)

Decrypt a file (prompt for key, restore stored name):
  cyph secret.txt.cyph
  (asks for key, writes restored filename via -d behavior)

Encrypt with a key file:
  cyph secret.txt mykey.txt
  (shorthand => -f secret.txt -k mykey.txt)

Decrypt with a key file:
  cyph secret.txt.cyph mykey.txt
  (shorthand => -i secret.txt.cyph -k mykey.txt -d)

2) Key sources (-k)
-------------------
You can provide key material in three ways:

  -k <file>
     If <file> ends with .txt, cyph normalizes the content:
       * lowercase
       * remove all whitespace
     Otherwise, cyph uses raw bytes from the file.

  -k=<text>
     Inline text key, normalized the same way as .txt.

  -k?
     Prompt a key from stdin, then normalize (lowercase + remove whitespace).

NOTE: normalization makes keys easier to type/copy but also changes what the "real" password is.
Example: "My Key 123" becomes "mykey123".

3) Wrapped key files (.cyphkey) and master keys (-K)
-----------------------------------------------------
A .cyphkey is a cyph container whose payload bytes are a key.
cyph never prints or writes the decrypted contents of a .cyphkey.

Usage:
  cyph -i data.cyph -k data.cyphkey -K? -d
or shorthand:
  cyph data.cyph data.cyphkey

Master key sources are analogous to -k:
  -K <file>    /  -K=<text>  /  -K?

If you use -k <something.cyphkey> then you MUST provide a master key explicitly (-K...).

4) Creating a .cyphkey (wrapping a key file)
--------------------------------------------
Wrap an existing key file (binary or text) into a .cyphkey:

  cyph -key rawkey.bin -o mywrapped.cyphkey -K?

or with a master key file:
  cyph -key rawkey.bin -o mywrapped -K master.txt
  (extension ".cyphkey" is auto-added)

Important: the bytes inside the key file are wrapped AS-IS (no normalization here).
This is useful if you keep high-entropy binary keys.

5) KDF hardness (-level)
------------------------
cyph derives an encryption key from your key material using libsodium crypto_pwhash.
-level sets pwhash cost parameters:

  -level 0 : interactive (fast, still safe for typical use)
  -level 1 : moderate
  -level 2 : sensitive (slowest / strongest)

Internally, cyph stores opslimit+memlimit in the .cyph/.cyphkey header, so decrypt can
always reproduce the same derived key WITHOUT you remembering -level.

6) -anon (privacy of filenames)
-------------------------------
cyph stores the original filename in the first encrypted frame (meta frame),
so it can restore the name on decrypt with -d.

If you use -anon, the stored (encrypted) name becomes "anonymous".
This avoids leaking real filenames even to someone who later decrypts.

7) How the container works (internals)
--------------------------------------
Container layout:

  MAGIC[8] + algo_ver(1) + flags(1) + opslimit(8) + memlimit(8)
  + SALT(16) + secretstream_header(24)
  + frames...

Frames are:
  uint32 length + ciphertext[length]

The first encrypted frame is the meta filename.
Remaining frames are file data encrypted in chunks using:
  crypto_secretstream_xchacha20poly1305 (XChaCha20-Poly1305 streaming AEAD)

This gives confidentiality + integrity (tampering is detected).

Key derivation:
  derived_key = crypto_pwhash(key_material_normalized, salt, opslimit, memlimit, alg_default)

Then derived_key initializes secretstream.

8) -WIPE (deletion)
-------------------
After successful encryption:
  - deletes original input files (those passed via -f)
  - deletes on-disk key file if you used -k <file>

After successful decryption:
  - deletes on-disk key file if you used -k <file>

This is a best-effort "delete". Secure deletion is not guaranteed on SSDs/journaling FS.

9) Shorthand rules (no directives)
----------------------------------
If you pass only positional files (no options starting with '-'):

  cyph file.cyph
    -> decrypt, ask key (-k?), restore name (-d)

  cyph file.cyph key.cyphkey
    -> decrypt with wrapped key, ask master key (-K?), restore name (-d)

  cyph file.cyph keyfile
    -> decrypt with keyfile, restore name (-d)

  cyph plaintext keyfile
    -> encrypt plaintext using keyfile

  cyph plaintext
    -> encrypt plaintext, ask key (-k?)

  cyph plaintext keyfile   (both not .cyph)
    -> encrypt first using second as keyfile

That’s it.
)";

/**
 * @brief Print brief help to stderr.
 */
static void usage_help() {
    std::cerr <<
R"(cyph - streaming file encryption tool (.cyph) + wrapped keyfiles (.cyphkey)

USAGE:
  cyph --version
  cyph --help
  cyph -man
  cyph -manprint
  cyph -gen [-o <path>]

ENCRYPT:
  cyph -f <file...> -k <keyfile|key.cyphkey> [-o <out_file_or_dir>] [-level N] [-anon] [-WIPE]
  cyph -f <file...> -k=<text>               [-o ...] [-level N] [-anon]
  cyph -f <file...> -k?                     [-o ...] [-level N] [-anon]

DECRYPT:
  cyph -i <file.cyph...> -k <keyfile|key.cyphkey> [-o ...] [-d] [-s] [-WIPE]
  cyph -i <file.cyph...> -k=<text>                [-o ...] [-d] [-s]
  cyph -i <file.cyph...> -k?                      [-o ...] [-d] [-s]

WRAPPED KEY (.cyphkey):
  If -k <file.cyphkey>, cyph decrypts it into RAM to obtain real key material.
  Requires master key: -K <file> / -K=<text> / -K?

CREATE .cyphkey (wrap a key file):
  cyph -key <keyfile> -o <name_or_path> (-K <file> | -K=<text> | -K?) [-level N]

OPTIONS:
  -level N   : 0=interactive, 1=moderate, 2=sensitive (stored in header)
  -anon      : encrypt only; store encrypted meta filename as "anonymous"
  -WIPE      : after SUCCESS delete originals (encrypt) and/or on-disk key file (-k <file>)

SHORTHAND (no directives; positional files only):
  cyph file.cyph
  cyph file.cyph key.cyphkey
  cyph file.cyph keyfile
  cyph plaintext keyfile
  cyph plaintext

Run: cyph -man  (full tutorial + internals)
)";
}

/**
 * @brief Print full manual to stderr.
 */
static void usage_man() {
    std::cerr << MANUAL_TEXT;
}

/**
 * @brief Write full manual to MANUAL_FILE.
 * @throws std::runtime_error on failure.
 */
static void write_manual_file() {
    std::ofstream f(MANUAL_FILE, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error(std::string("Cannot open for writing: ") + MANUAL_FILE);
    f.write(MANUAL_TEXT, static_cast<std::streamsize>(std::strlen(MANUAL_TEXT)));
    if (!f) throw std::runtime_error(std::string("Failed to write: ") + MANUAL_FILE);
}

/* ============================
 * Crypto format + KDF
 * ============================ */

/** @brief Container magic for .cyph / .cyphkey. */
static const unsigned char MAGIC[8] = {'M','Y','C','Y','P','H','S','3'};
/** @brief Algorithm version (container semantics). */
static constexpr uint8_t ALGO_VER = 1;
/** @brief Salt length. */
static constexpr size_t SALT_LEN = crypto_pwhash_SALTBYTES;

/** @brief Header flag: meta filename is "anonymous". */
static constexpr uint8_t FLAG_ANON_META = 1u << 0;

/**
 * @brief KDF parameters (pwhash).
 */
struct KdfParams {
    unsigned long long opslimit;
    size_t memlimit;
};

/**
 * @brief Map -level to libsodium pwhash parameters.
 */
static KdfParams kdf_params_for_level(int level) {
    if (level < 0) level = 0;
    if (level > 2) level = 2;

    switch (level) {
        case 0:
            return {crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE};
        case 1:
            return {crypto_pwhash_OPSLIMIT_MODERATE, crypto_pwhash_MEMLIMIT_MODERATE};
        case 2:
        default:
            return {crypto_pwhash_OPSLIMIT_SENSITIVE, crypto_pwhash_MEMLIMIT_SENSITIVE};
    }
}

/**
 * @brief Derive secretstream key using libsodium crypto_pwhash (Argon2id default).
 */
static void derive_key(unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES],
                       const unsigned char salt[SALT_LEN],
                       const std::vector<unsigned char>& key_material,
                       const KdfParams& kdf) {
    if (key_material.empty()) throw std::runtime_error("Key material is empty");

    const char* pw = reinterpret_cast<const char*>(key_material.data());
    const size_t pwlen = key_material.size();

    if (crypto_pwhash(key, crypto_secretstream_xchacha20poly1305_KEYBYTES,
                      pw, pwlen, salt,
                      kdf.opslimit, kdf.memlimit,
                      crypto_pwhash_ALG_DEFAULT) != 0) {
        throw std::runtime_error("crypto_pwhash failed (OOM?)");
    }
}

/**
 * @brief Load key material from a file.
 * If file ends with .txt, normalize contents; otherwise use raw bytes.
 */
static std::vector<unsigned char> key_from_file(const std::string& key_path) {
    auto data = read_file_bytes(key_path);
    if (has_ext(key_path, ".txt")) {
        std::string text(reinterpret_cast<const char*>(data.data()), data.size());
        std::string norm = lowercase_no_ws(text);
        if (!text.empty()) sodium_memzero(text.data(), text.size());

        std::vector<unsigned char> out(norm.begin(), norm.end());
        if (!norm.empty()) sodium_memzero(norm.data(), norm.size());
        secure_wipe(data);
        return out;
    }
    return data;
}

/**
 * @brief Normalize inline text key and return bytes.
 */
static std::vector<unsigned char> key_from_inline_text_normalized(const std::string& inline_key) {
    std::string norm = lowercase_no_ws(inline_key);
    std::vector<unsigned char> out(norm.begin(), norm.end());
    if (!norm.empty()) sodium_memzero(norm.data(), norm.size());
    return out;
}

/* ============================
 * CLI types
 * ============================ */

/** @brief Program mode. */
enum class Mode { None, Encrypt, Decrypt, Gen, KeyWrap };
/** @brief Key source type. */
enum class KeySrc { None, File, Inline, Prompt };

/**
 * @brief Key specification for -k / -K.
 */
struct KeySpec {
    KeySrc src = KeySrc::None;
    std::string path;
    std::string inline_s;

    bool is_set() const { return src != KeySrc::None; }
    bool is_file() const { return src == KeySrc::File; }
};

/**
 * @brief Parsed arguments.
 */
struct Args {
    bool show_help = false;
    bool show_version = false;
    bool show_man = false;
    bool man_print = false;

    Mode mode = Mode::None;

    std::vector<std::string> enc_inputs;  ///< -f
    std::vector<std::string> dec_inputs;  ///< -i

    std::string out;            ///< -o
    bool restore_name = false;  ///< -d
    bool show_stdout = false;   ///< -s
    bool wipe = false;          ///< -WIPE
    bool anon = false;          ///< -anon
    int level = 0;              ///< -level

    KeySpec key;                ///< -k
    KeySpec master;             ///< -K (required for .cyphkey unwrap and -key wrap)

    bool gen = false;           ///< -gen

    bool keywrap = false;       ///< -key <file>
    std::string keywrap_input_keyfile;
};

/**
 * @brief True if argv contains only positional args (no directives).
 */
static bool all_positional_no_directives(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (looks_like_option(a)) return false;
    }
    return argc >= 2;
}

/**
 * @brief Parse arguments, including shorthand positional mode.
 * @throws std::runtime_error for invalid usage.
 */
static Args parse_args(int argc, char** argv) {
    Args a;

    auto need = [&](int& i, const std::string& opt) -> std::string {
        if (i + 1 >= argc) throw std::runtime_error("Missing value for " + opt);
        return std::string(argv[++i]);
    };

    auto parse_o_inline = [&](const std::string& arg, int& i) {
        if (arg == "-o") {
            a.out = need(i, "-o");
            return true;
        }
        if (arg.rfind("-o", 0) == 0 && arg.size() > 2) {
            a.out = arg.substr(2);
            return true;
        }
        return false;
    };

    // Shorthand: only positional files, no options.
    if (all_positional_no_directives(argc, argv)) {
        if (argc == 2) {
            std::string f1 = argv[1];
            if (has_ext(f1, EXT_CYPH)) {
                a.mode = Mode::Decrypt;
                a.dec_inputs = {f1};
                a.key.src = KeySrc::Prompt; // -k?
                a.restore_name = true;       // -d
                return a;
            }
            a.mode = Mode::Encrypt;
            a.enc_inputs = {f1};
            a.key.src = KeySrc::Prompt; // -k?
            return a;
        }

        if (argc == 3) {
            std::string p1 = argv[1];
            std::string p2 = argv[2];

            const bool p1_cyph = has_ext(p1, EXT_CYPH);
            const bool p2_cyph = has_ext(p2, EXT_CYPH);

            if (p1_cyph ^ p2_cyph) {
                std::string cyph = p1_cyph ? p1 : p2;
                std::string keyf = p1_cyph ? p2 : p1;

                a.mode = Mode::Decrypt;
                a.dec_inputs = {cyph};
                a.key.src = KeySrc::File;
                a.key.path = keyf;
                a.restore_name = true; // -d

                if (has_ext(keyf, EXT_CYPHKEY)) {
                    a.master.src = KeySrc::Prompt; // -K?
                }
                return a;
            }

            // Two non-.cyph files => encrypt first using second as keyfile.
            if (!p1_cyph && !p2_cyph) {
                a.mode = Mode::Encrypt;
                a.enc_inputs = {p1};
                a.key.src = KeySrc::File;
                a.key.path = p2;
                return a;
            }

            throw std::runtime_error("Shorthand expects either: (file.cyph [key]) or (plaintext [key])");
        }

        throw std::runtime_error("Shorthand supports only 1 or 2 positional files");
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--version") {
            a.show_version = true;
        } else if (arg == "-h" || arg == "-help" || arg == "--help") {
            a.show_help = true;
        } else if (arg == "-man") {
            a.show_man = true;
        } else if (arg == "-manprint") {
            a.man_print = true;
        } else if (arg == "-gen") {
            a.gen = true;
            a.mode = Mode::Gen;
        } else if (arg == "-s") {
            a.show_stdout = true;
        } else if (arg == "-d") {
            a.restore_name = true;
        } else if (arg == "-WIPE") {
            a.wipe = true;
        } else if (arg == "-anon") {
            a.anon = true;
        } else if (arg == "-level") {
            std::string v = need(i, "-level");
            try {
                a.level = std::stoi(v);
            } catch (...) {
                throw std::runtime_error("Invalid value for -level (expected integer 0..2)");
            }
        } else if (parse_o_inline(arg, i)) {
            // handled
        } else if (arg == "-k") {
            a.key.src = KeySrc::File;
            a.key.path = need(i, "-k");
        } else if (arg.rfind("-k=", 0) == 0) {
            a.key.src = KeySrc::Inline;
            a.key.inline_s = arg.substr(3);
        } else if (arg == "-k?") {
            a.key.src = KeySrc::Prompt;
        } else if (arg == "-K") {
            a.master.src = KeySrc::File;
            a.master.path = need(i, "-K");
        } else if (arg.rfind("-K=", 0) == 0) {
            a.master.src = KeySrc::Inline;
            a.master.inline_s = arg.substr(3);
        } else if (arg == "-K?") {
            a.master.src = KeySrc::Prompt;
        } else if (arg == "-key") {
            a.keywrap = true;
            a.mode = Mode::KeyWrap;
            a.keywrap_input_keyfile = need(i, "-key");
        } else if (arg == "-f") {
            a.mode = Mode::Encrypt;
            while (i + 1 < argc && !looks_like_option(argv[i + 1])) {
                a.enc_inputs.emplace_back(argv[++i]);
            }
            if (a.enc_inputs.empty()) throw std::runtime_error("No paths after -f");
        } else if (arg == "-i") {
            a.mode = Mode::Decrypt;
            while (i + 1 < argc && !looks_like_option(argv[i + 1])) {
                a.dec_inputs.emplace_back(argv[++i]);
            }
            if (a.dec_inputs.empty()) throw std::runtime_error("No paths after -i");
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (a.show_help || a.show_version || a.show_man || a.man_print) return a;
    if (a.mode == Mode::Gen) return a;

    if (a.mode == Mode::KeyWrap) {
        if (a.keywrap_input_keyfile.empty()) throw std::runtime_error("Missing key file for -key");
        if (a.out.empty()) throw std::runtime_error("Missing -o <name_or_path> for -key");
        if (!a.master.is_set()) throw std::runtime_error("Master key required for -key: -K <file> / -K=<text> / -K?");
        return a;
    }

    const bool enc = !a.enc_inputs.empty();
    const bool dec = !a.dec_inputs.empty();
    if (enc == dec) throw std::runtime_error("Choose exactly one mode: -f (encrypt) OR -i (decrypt)");
    if (!a.key.is_set()) throw std::runtime_error("Key required: -k <file> / -k=<text> / -k?");

    if (a.anon && !enc) throw std::runtime_error("-anon is only valid with -f (encrypt)");

    if (a.key.is_file() && has_ext(a.key.path, EXT_CYPHKEY) && !a.master.is_set()) {
        throw std::runtime_error("Using -k <file.cyphkey> requires master key: -K <file> / -K=<text> / -K?");
    }

    return a;
}

/* ============================
 * Container header + streaming crypto
 * ============================ */

static constexpr size_t CHUNK = 1u << 20;

/**
 * @brief On-disk container header (fixed layout).
 */
struct ContainerHeader {
    uint8_t algo_ver = 0;
    uint8_t flags = 0;
    unsigned long long opslimit = 0;
    unsigned long long memlimit_u64 = 0;
    unsigned char salt[SALT_LEN]{};
    unsigned char ss_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES]{};
};

/**
 * @brief Write container header to output stream.
 */
static void write_container_header(std::ofstream& out, const ContainerHeader& h) {
    out.write(reinterpret_cast<const char*>(MAGIC), sizeof(MAGIC));
    out.put(static_cast<char>(h.algo_ver));
    out.put(static_cast<char>(h.flags));
    out.write(reinterpret_cast<const char*>(&h.opslimit), sizeof(h.opslimit));
    out.write(reinterpret_cast<const char*>(&h.memlimit_u64), sizeof(h.memlimit_u64));
    out.write(reinterpret_cast<const char*>(h.salt), sizeof(h.salt));
    out.write(reinterpret_cast<const char*>(h.ss_header), sizeof(h.ss_header));
    if (!out) throw std::runtime_error("Failed to write header");
}

/**
 * @brief Read and validate container header.
 */
static ContainerHeader read_container_header(std::ifstream& in, const std::string& path_for_err) {
    ContainerHeader h{};
    unsigned char magic[sizeof(MAGIC)];

    in.read(reinterpret_cast<char*>(magic), sizeof(magic));
    if (!in) throw std::runtime_error("Invalid/corrupt file header: " + path_for_err);

    if (sodium_memcmp(magic, MAGIC, sizeof(MAGIC)) != 0) {
        throw std::runtime_error("Bad magic/version: " + path_for_err);
    }

    int av = in.get();
    int fl = in.get();
    if (av == EOF || fl == EOF) throw std::runtime_error("Corrupt header: " + path_for_err);
    h.algo_ver = static_cast<uint8_t>(av);
    h.flags = static_cast<uint8_t>(fl);

    in.read(reinterpret_cast<char*>(&h.opslimit), sizeof(h.opslimit));
    in.read(reinterpret_cast<char*>(&h.memlimit_u64), sizeof(h.memlimit_u64));
    in.read(reinterpret_cast<char*>(h.salt), sizeof(h.salt));
    in.read(reinterpret_cast<char*>(h.ss_header), sizeof(h.ss_header));
    if (!in) throw std::runtime_error("Invalid/corrupt file header: " + path_for_err);

    if (h.algo_ver != ALGO_VER) {
        throw std::runtime_error("Unsupported algo version in file: " + path_for_err);
    }

    return h;
}

/**
 * @brief Extract KDF parameters from header.
 */
static KdfParams kdf_from_header(const ContainerHeader& h) {
    KdfParams k{h.opslimit, static_cast<size_t>(h.memlimit_u64)};
    if (k.memlimit == 0 || k.opslimit == 0) throw std::runtime_error("Corrupt KDF params in header");
    return k;
}

/**
 * @brief Create encryption output path based on -o and number of inputs.
 */
static std::string make_encrypt_out_path(const std::string& in,
                                         const Args& args,
                                         bool multiple) {
    if (args.out.empty()) return ensure_cyph_ext(in);

    fs::path outp(args.out);
    if (multiple) {
        ensure_dir_exists(outp);
        std::string name = ensure_cyph_ext(fs::path(in).filename().string());
        return (outp / name).string();
    }

    fs::path parent = outp.parent_path();
    if (!parent.empty()) ensure_dir_exists(parent);
    return ensure_cyph_ext(args.out);
}

/**
 * @brief Create decryption output path based on -o / -d / multiple inputs.
 */
static std::string make_decrypt_out_path(const std::string& enc_in,
                                         const Args& args,
                                         bool multiple,
                                         const std::string& restored_name) {
    if (!args.out.empty()) {
        fs::path outp(args.out);
        if (multiple) {
            ensure_dir_exists(outp);
            fs::path fname = args.restore_name
                                 ? fs::path(restored_name)
                                 : fs::path(enc_in).filename().replace_extension("");
            return (outp / fname).string();
        }

        fs::path parent = outp.parent_path();
        if (!parent.empty()) ensure_dir_exists(parent);
        return args.out;
    }

    if (args.restore_name) {
        fs::path dir = fs::path(enc_in).parent_path();
        if (!dir.empty()) ensure_dir_exists(dir);
        return (dir / restored_name).string();
    }

    return "";
}

/**
 * @brief Encrypt a file into a .cyph container.
 */
static void encrypt_file_stream(const std::string& in_path,
                                const std::string& out_path,
                                const std::vector<unsigned char>& key_material,
                                const KdfParams& kdf,
                                uint8_t flags,
                                const std::string& meta_name) {
    std::ifstream in(in_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + in_path);

    fs::path outp(out_path);
    if (!outp.parent_path().empty()) ensure_dir_exists(outp.parent_path());

    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open output: " + out_path);

    ContainerHeader h{};
    h.algo_ver = ALGO_VER;
    h.flags = flags;
    h.opslimit = kdf.opslimit;
    h.memlimit_u64 = static_cast<unsigned long long>(kdf.memlimit);
    randombytes_buf(h.salt, sizeof h.salt);

    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    derive_key(key, h.salt, key_material, kdf);

    crypto_secretstream_xchacha20poly1305_state st;
    if (crypto_secretstream_xchacha20poly1305_init_push(&st, h.ss_header, key) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("secretstream init_push failed");
    }

    write_container_header(out, h);

    std::vector<unsigned char> meta(meta_name.begin(), meta_name.end());
    std::vector<unsigned char> meta_ct(meta.size() + crypto_secretstream_xchacha20poly1305_ABYTES);
    unsigned long long meta_ct_len = 0;

    if (crypto_secretstream_xchacha20poly1305_push(
            &st, meta_ct.data(), &meta_ct_len,
            meta.data(), meta.size(),
            nullptr, 0,
            crypto_secretstream_xchacha20poly1305_TAG_MESSAGE) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("secretstream push(meta) failed");
    }

    meta_ct.resize(static_cast<size_t>(meta_ct_len));
    uint32_t L = static_cast<uint32_t>(meta_ct.size());
    out.write(reinterpret_cast<const char*>(&L), sizeof(L));
    out.write(reinterpret_cast<const char*>(meta_ct.data()),
              static_cast<std::streamsize>(meta_ct.size()));
    if (!out) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Failed to write meta frame: " + out_path);
    }

    std::vector<unsigned char> buf(CHUNK);
    std::vector<unsigned char> ct(CHUNK + crypto_secretstream_xchacha20poly1305_ABYTES);

    while (true) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = in.gcount();
        if (got < 0) got = 0;

        const bool eof = in.eof();
        unsigned char tag = eof ? crypto_secretstream_xchacha20poly1305_TAG_FINAL
                                : crypto_secretstream_xchacha20poly1305_TAG_MESSAGE;

        unsigned long long ct_len = 0;
        if (crypto_secretstream_xchacha20poly1305_push(
                &st, ct.data(), &ct_len,
                buf.data(), static_cast<size_t>(got),
                nullptr, 0,
                tag) != 0) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("secretstream push(data) failed");
        }

        uint32_t len32 = static_cast<uint32_t>(ct_len);
        out.write(reinterpret_cast<const char*>(&len32), sizeof(len32));
        out.write(reinterpret_cast<const char*>(ct.data()), static_cast<std::streamsize>(ct_len));
        if (!out) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("Failed to write ciphertext frame");
        }

        if (eof) break;
    }

    sodium_memzero(key, sizeof key);
}

/**
 * @brief Read only the decrypted meta filename (first frame).
 */
static std::string read_restored_name_quick(const std::string& enc_in,
                                            const std::vector<unsigned char>& key_material) {
    std::ifstream in(enc_in, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + enc_in);

    ContainerHeader h = read_container_header(in, enc_in);
    KdfParams kdf = kdf_from_header(h);

    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    derive_key(key, h.salt, key_material, kdf);

    crypto_secretstream_xchacha20poly1305_state st;
    if (crypto_secretstream_xchacha20poly1305_init_pull(&st, h.ss_header, key) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("secretstream init_pull failed");
    }

    uint32_t len32 = 0;
    in.read(reinterpret_cast<char*>(&len32), sizeof(len32));
    if (!in) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Missing meta frame");
    }

    std::vector<unsigned char> meta_ct(len32);
    in.read(reinterpret_cast<char*>(meta_ct.data()),
            static_cast<std::streamsize>(meta_ct.size()));
    if (!in) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Corrupt meta frame");
    }

    std::vector<unsigned char> meta_pt(meta_ct.size());
    unsigned long long meta_pt_len = 0;
    unsigned char tag = 0;

    if (crypto_secretstream_xchacha20poly1305_pull(
            &st, meta_pt.data(), &meta_pt_len,
            &tag,
            meta_ct.data(), meta_ct.size(),
            nullptr, 0) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Wrong key or corrupt file (meta)");
    }

    meta_pt.resize(static_cast<size_t>(meta_pt_len));
    std::string restored(reinterpret_cast<const char*>(meta_pt.data()), meta_pt.size());
    sodium_memzero(key, sizeof key);
    return restored;
}

/**
 * @brief Decrypt a .cyph container to file and/or stdout.
 */
static void decrypt_file_stream(const std::string& enc_path,
                                const std::string& out_path_or_empty,
                                const std::vector<unsigned char>& key_material,
                                bool to_stdout,
                                bool write_file) {
    std::ifstream in(enc_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + enc_path);

    ContainerHeader h = read_container_header(in, enc_path);
    KdfParams kdf = kdf_from_header(h);

    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    derive_key(key, h.salt, key_material, kdf);

    crypto_secretstream_xchacha20poly1305_state st;
    if (crypto_secretstream_xchacha20poly1305_init_pull(&st, h.ss_header, key) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("secretstream init_pull failed");
    }

    auto read_frame = [&](std::vector<unsigned char>& frame) -> bool {
        uint32_t len32 = 0;
        in.read(reinterpret_cast<char*>(&len32), sizeof(len32));
        if (!in) return false;
        frame.resize(len32);
        in.read(reinterpret_cast<char*>(frame.data()), static_cast<std::streamsize>(len32));
        if (!in) throw std::runtime_error("Corrupt ciphertext frame");
        return true;
    };

    std::vector<unsigned char> meta_ct;
    if (!read_frame(meta_ct)) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Missing meta frame");
    }

    std::vector<unsigned char> meta_pt(meta_ct.size());
    unsigned long long meta_pt_len = 0;
    unsigned char meta_tag = 0;
    if (crypto_secretstream_xchacha20poly1305_pull(
            &st, meta_pt.data(), &meta_pt_len, &meta_tag,
            meta_ct.data(), meta_ct.size(),
            nullptr, 0) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Wrong key or corrupt file (meta)");
    }

    std::ofstream out;
    if (write_file && !out_path_or_empty.empty()) {
        fs::path outp(out_path_or_empty);
        if (!outp.parent_path().empty()) ensure_dir_exists(outp.parent_path());
        out.open(out_path_or_empty, std::ios::binary | std::ios::trunc);
        if (!out) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("Cannot open output: " + out_path_or_empty);
        }
    }

    std::vector<unsigned char> ct;
    std::vector<unsigned char> pt(CHUNK + crypto_secretstream_xchacha20poly1305_ABYTES);

    bool done = false;
    while (!done) {
        if (!read_frame(ct)) break;

        unsigned long long pt_len = 0;
        unsigned char tag = 0;

        if (crypto_secretstream_xchacha20poly1305_pull(
                &st,
                pt.data(), &pt_len,
                &tag,
                ct.data(), ct.size(),
                nullptr, 0) != 0) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("Wrong key or corrupt file (data)");
        }

        if (pt_len > 0) {
            if (to_stdout) write_stdout(pt.data(), static_cast<size_t>(pt_len));
            if (write_file && out) {
                out.write(reinterpret_cast<const char*>(pt.data()),
                          static_cast<std::streamsize>(pt_len));
                if (!out) {
                    sodium_memzero(key, sizeof key);
                    throw std::runtime_error("Failed to write output file");
                }
            }
        }

        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) done = true;
    }

    if (!done) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Corrupt file: missing FINAL tag");
    }

    sodium_memzero(key, sizeof key);
}

/**
 * @brief Decrypt container payload to bytes in memory (used for .cyphkey unwrap).
 * Meta frame is consumed and discarded.
 */
static std::vector<unsigned char> decrypt_payload_to_bytes(const std::string& enc_path,
                                                           const std::vector<unsigned char>& key_material) {
    std::ifstream in(enc_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + enc_path);

    ContainerHeader h = read_container_header(in, enc_path);
    KdfParams kdf = kdf_from_header(h);

    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    derive_key(key, h.salt, key_material, kdf);

    crypto_secretstream_xchacha20poly1305_state st;
    if (crypto_secretstream_xchacha20poly1305_init_pull(&st, h.ss_header, key) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("secretstream init_pull failed");
    }

    auto read_frame = [&](std::vector<unsigned char>& frame) -> bool {
        uint32_t len32 = 0;
        in.read(reinterpret_cast<char*>(&len32), sizeof(len32));
        if (!in) return false;
        frame.resize(len32);
        in.read(reinterpret_cast<char*>(frame.data()), static_cast<std::streamsize>(len32));
        if (!in) throw std::runtime_error("Corrupt ciphertext frame");
        return true;
    };

    std::vector<unsigned char> meta_ct;
    if (!read_frame(meta_ct)) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Missing meta frame");
    }
    std::vector<unsigned char> meta_pt(meta_ct.size());
    unsigned long long meta_pt_len = 0;
    unsigned char meta_tag = 0;
    if (crypto_secretstream_xchacha20poly1305_pull(
            &st, meta_pt.data(), &meta_pt_len, &meta_tag,
            meta_ct.data(), meta_ct.size(),
            nullptr, 0) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Wrong key or corrupt file (meta)");
    }

    std::vector<unsigned char> out_bytes;
    std::vector<unsigned char> ct;
    std::vector<unsigned char> pt(CHUNK + crypto_secretstream_xchacha20poly1305_ABYTES);

    bool done = false;
    while (!done) {
        if (!read_frame(ct)) break;

        unsigned long long pt_len = 0;
        unsigned char tag = 0;

        if (crypto_secretstream_xchacha20poly1305_pull(
                &st,
                pt.data(), &pt_len,
                &tag,
                ct.data(), ct.size(),
                nullptr, 0) != 0) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("Wrong key or corrupt file (data)");
        }

        if (pt_len > 0) {
            const size_t old = out_bytes.size();
            out_bytes.resize(old + static_cast<size_t>(pt_len));
            std::memcpy(out_bytes.data() + old, pt.data(), static_cast<size_t>(pt_len));
        }

        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) done = true;
    }

    if (!done) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Corrupt file: missing FINAL tag");
    }

    sodium_memzero(key, sizeof key);
    return out_bytes;
}

/* ============================
 * Key resolution and .cyphkey wrapping
 * ============================ */

/**
 * @brief Convert a KeySpec to key material bytes (normalized for text forms).
 */
static std::vector<unsigned char> material_from_keyspec_normalized(const KeySpec& ks,
                                                                   const std::string& prompt) {
    if (ks.src == KeySrc::File) return key_from_file(ks.path);
    if (ks.src == KeySrc::Inline) return key_from_inline_text_normalized(ks.inline_s);
    if (ks.src == KeySrc::Prompt) return prompt_key_normalized(prompt);
    throw std::runtime_error("Key not provided");
}

/**
 * @brief Resolve main key material, unwrapping .cyphkey if required.
 */
static std::vector<unsigned char> resolve_main_key_material(const Args& args) {
    if (args.key.src == KeySrc::File && has_ext(args.key.path, EXT_CYPHKEY)) {
        std::vector<unsigned char> master_mat =
            material_from_keyspec_normalized(args.master, "Enter master key (-K?): ");
        std::vector<unsigned char> wrapped_plain = decrypt_payload_to_bytes(args.key.path, master_mat);
        secure_wipe(master_mat);
        return wrapped_plain;
    }
    return material_from_keyspec_normalized(args.key, "Enter key (-k?): ");
}

/**
 * @brief Resolve master key material.
 */
static std::vector<unsigned char> resolve_master_key_material(const Args& args) {
    return material_from_keyspec_normalized(args.master, "Enter master key (-K?): ");
}

/**
 * @brief Create a .cyphkey container by encrypting bytes from input_keyfile using master key.
 */
static void create_cyphkey_file(const std::string& input_keyfile,
                                const std::string& out_name_or_path,
                                const std::vector<unsigned char>& master_key_material,
                                const KdfParams& kdf) {
    std::vector<unsigned char> plain = read_file_bytes(input_keyfile);
    std::string out_path = ensure_cyphkey_ext(out_name_or_path);

    fs::path outp(out_path);
    if (!outp.parent_path().empty()) ensure_dir_exists(outp.parent_path());

    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open output: " + out_path);

    ContainerHeader h{};
    h.algo_ver = ALGO_VER;
    h.flags = 0;
    h.opslimit = kdf.opslimit;
    h.memlimit_u64 = static_cast<unsigned long long>(kdf.memlimit);
    randombytes_buf(h.salt, sizeof h.salt);

    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    derive_key(key, h.salt, master_key_material, kdf);

    crypto_secretstream_xchacha20poly1305_state st;
    if (crypto_secretstream_xchacha20poly1305_init_push(&st, h.ss_header, key) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("secretstream init_push failed");
    }

    write_container_header(out, h);

    const std::string meta_name = "cyphkey";
    std::vector<unsigned char> meta(meta_name.begin(), meta_name.end());
    std::vector<unsigned char> meta_ct(meta.size() + crypto_secretstream_xchacha20poly1305_ABYTES);
    unsigned long long meta_ct_len = 0;
    if (crypto_secretstream_xchacha20poly1305_push(
            &st, meta_ct.data(), &meta_ct_len,
            meta.data(), meta.size(),
            nullptr, 0,
            crypto_secretstream_xchacha20poly1305_TAG_MESSAGE) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("secretstream push(meta) failed");
    }
    meta_ct.resize(static_cast<size_t>(meta_ct_len));
    uint32_t L = static_cast<uint32_t>(meta_ct.size());
    out.write(reinterpret_cast<const char*>(&L), sizeof(L));
    out.write(reinterpret_cast<const char*>(meta_ct.data()),
              static_cast<std::streamsize>(meta_ct.size()));
    if (!out) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Failed to write meta frame: " + out_path);
    }

    std::vector<unsigned char> ct(CHUNK + crypto_secretstream_xchacha20poly1305_ABYTES);
    size_t off = 0;

    while (off < plain.size()) {
        size_t take = std::min(CHUNK, plain.size() - off);
        unsigned char tag = ((off + take) == plain.size())
                                ? crypto_secretstream_xchacha20poly1305_TAG_FINAL
                                : crypto_secretstream_xchacha20poly1305_TAG_MESSAGE;

        unsigned long long ct_len = 0;
        if (crypto_secretstream_xchacha20poly1305_push(
                &st, ct.data(), &ct_len,
                plain.data() + off, take,
                nullptr, 0,
                tag) != 0) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("secretstream push(data) failed");
        }

        uint32_t len32 = static_cast<uint32_t>(ct_len);
        out.write(reinterpret_cast<const char*>(&len32), sizeof(len32));
        out.write(reinterpret_cast<const char*>(ct.data()), static_cast<std::streamsize>(ct_len));
        if (!out) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("Failed to write ciphertext frame");
        }

        off += take;
    }

    if (plain.empty()) {
        unsigned long long ct_len = 0;
        if (crypto_secretstream_xchacha20poly1305_push(
                &st, ct.data(), &ct_len,
                nullptr, 0,
                nullptr, 0,
                crypto_secretstream_xchacha20poly1305_TAG_FINAL) != 0) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("secretstream push(final-empty) failed");
        }
        uint32_t len32 = static_cast<uint32_t>(ct_len);
        out.write(reinterpret_cast<const char*>(&len32), sizeof(len32));
        out.write(reinterpret_cast<const char*>(ct.data()), static_cast<std::streamsize>(ct_len));
        if (!out) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("Failed to write ciphertext frame");
        }
    }

    sodium_memzero(key, sizeof key);
    secure_wipe(plain);
}

/* ============================
 * -gen
 * ============================ */

/**
 * @brief Generate a random printable key (lowercase alnum).
 */
static std::string gen_key_text(size_t len = 64) {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static constexpr size_t A = sizeof(alphabet) - 1;

    std::string out;
    out.resize(len);

    std::vector<unsigned char> rnd(len);
    randombytes_buf(rnd.data(), rnd.size());

    for (size_t i = 0; i < len; i++) out[i] = alphabet[rnd[i] % A];
    secure_wipe(rnd);
    return out;
}

/* ============================
 * main
 * ============================ */

int main(int argc, char** argv) {
    if (sodium_init() < 0) {
        std::cerr << "sodium_init failed\n";
        return 1;
    }

    try {
        Args args = parse_args(argc, argv);

        if (args.show_version) {
            std::cout << "cyph " << VERSION << "\n";
            return 0;
        }
        if (args.show_help) {
            usage_help();
            return 0;
        }
        if (args.show_man) {
            usage_man();
            return 0;
        }
        if (args.man_print) {
            write_manual_file();
            std::cout << "Written: " << MANUAL_FILE << "\n";
            return 0;
        }

        if (args.mode == Mode::Gen) {
            const std::string key = gen_key_text(64);
            if (!args.out.empty()) {
                write_file_bytes(args.out, std::vector<unsigned char>(key.begin(), key.end()));
                std::cout << "Generated key saved to: " << args.out << "\n";
            } else {
                std::cout << key << "\n";
            }
            return 0;
        }

        const KdfParams kdf = kdf_params_for_level(args.level);

        if (args.mode == Mode::KeyWrap) {
            std::vector<unsigned char> master = resolve_master_key_material(args);
            create_cyphkey_file(args.keywrap_input_keyfile, args.out, master, kdf);
            secure_wipe(master);
            std::cout << "Created wrapped key file: " << ensure_cyphkey_ext(args.out) << "\n";
            return 0;
        }

        std::vector<unsigned char> key_material = resolve_main_key_material(args);
        if (key_material.empty()) throw std::runtime_error("Key material is empty (after normalization)");

        const bool encrypt_mode = (args.mode == Mode::Encrypt);
        const bool multiple = encrypt_mode ? (args.enc_inputs.size() > 1)
                                           : (args.dec_inputs.size() > 1);

        bool do_wipe = false;
        std::vector<std::string> wipe_targets;

        if (args.wipe) {
            if (encrypt_mode) {
                wipe_targets = args.enc_inputs;
                if (args.key.src == KeySrc::File) wipe_targets.push_back(args.key.path);
            } else {
                if (args.key.src == KeySrc::File) wipe_targets.push_back(args.key.path);
            }

            if (!wipe_targets.empty()) {
                do_wipe = confirm_wipe(wipe_targets);
                if (!do_wipe) std::cerr << "WIPE cancelled by user.\n";
            } else {
                std::cerr << "Note: -WIPE requested, but there are no on-disk key files to delete.\n";
            }
        }

        if (encrypt_mode) {
            uint8_t flags = 0;
            if (args.anon) flags |= FLAG_ANON_META;

            for (const auto& in : args.enc_inputs) {
                const std::string outp = make_encrypt_out_path(in, args, multiple);
                const std::string meta_name = args.anon ? std::string("anonymous") : basename_only(in);
                encrypt_file_stream(in, outp, key_material, kdf, flags, meta_name);
                std::cout << "Encrypted: " << outp << "\n";
            }

            if (do_wipe) {
                bool any_fail = false;
                for (const auto& in : args.enc_inputs) {
                    if (!remove_file_best_effort(in)) {
                        any_fail = true;
                        std::cerr << "Failed to delete: " << in << "\n";
                    } else {
                        std::cout << "Deleted: " << in << "\n";
                    }
                }
                if (args.key.src == KeySrc::File) {
                    if (!remove_file_best_effort(args.key.path)) {
                        any_fail = true;
                        std::cerr << "Failed to delete key file: " << args.key.path << "\n";
                    } else {
                        std::cout << "Deleted key file: " << args.key.path << "\n";
                    }
                }
                if (any_fail) std::cerr << "WIPE completed with errors.\n";
                else std::cout << "WIPE completed.\n";
            }
        } else {
            bool any_decrypt_fail = false;

            for (const auto& enc_in : args.dec_inputs) {
                std::string restored = read_restored_name_quick(enc_in, key_material);

                const std::string outp = make_decrypt_out_path(enc_in, args, multiple, restored);
                const bool write_file = !outp.empty();
                const bool to_stdout = args.show_stdout;

                decrypt_file_stream(enc_in, outp, key_material, to_stdout, write_file);

                if (write_file) {
                    std::cout << "\nDecrypted saved to: " << outp << "\n";
                } else if (!to_stdout) {
                    std::cout << "Decryption successful (no output written).\n";
                }
            }

            if (do_wipe) {
                if (args.key.src == KeySrc::File) {
                    if (!remove_file_best_effort(args.key.path)) {
                        any_decrypt_fail = true;
                        std::cerr << "Failed to delete key file: " << args.key.path << "\n";
                    } else {
                        std::cout << "Deleted key file: " << args.key.path << "\n";
                    }
                }
                if (any_decrypt_fail) std::cerr << "WIPE completed with errors.\n";
                else std::cout << "WIPE completed.\n";
            }
        }

        secure_wipe(key_material);
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Run: cyph --help\n";
        return 2;
    }
}
