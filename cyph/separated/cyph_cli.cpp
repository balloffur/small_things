// client_terminal.cpp
#include "core.h"
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

static constexpr const char* MANUAL_FILE = "cyph_manual.txt";

static std::string strip_ws(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char ch : s) {
        if (std::isspace(ch)) continue;
        out.push_back(static_cast<char>(ch));
    }
    return out;
}

static bool has_ext(const std::string& path, const std::string& ext) {
    if (path.size() < ext.size()) return false;
    return path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
}

static bool looks_like_option(const std::string& s) {
    return !s.empty() && s[0] == '-';
}

static std::string lowercase_no_ws(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char ch : s) {
        if (std::isspace(ch)) continue;
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

static std::vector<unsigned char> read_file_bytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open for reading: " + path);

    f.seekg(0, std::ios::end);
    std::streamoff n = f.tellg();
    f.seekg(0, std::ios::beg);
    if (n < 0) throw std::runtime_error("Failed to stat: " + path);

    std::vector<unsigned char> buf(static_cast<std::size_t>(n));
    if (n > 0) {
        f.read(reinterpret_cast<char*>(buf.data()), n);
        if (!f) throw std::runtime_error("Failed to read: " + path);
    }
    return buf;
}

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

static void secure_wipe(std::vector<unsigned char>& v) {
    if (!v.empty()) sodium_memzero(v.data(), v.size());
    v.clear();
    v.shrink_to_fit();
}

static bool remove_file_best_effort(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

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

static std::string basename_only(const std::string& p) {
    return fs::path(p).filename().string();
}

static void ensure_dir_exists(const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) throw std::runtime_error("Failed to create directory: " + dir.string());
}

static std::string prompt_line(const std::string& prompt) {
    std::cerr << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

static std::vector<unsigned char> prompt_key_normalized(const std::string& prompt) {
    std::string s = prompt_line(prompt);
    std::string norm = lowercase_no_ws(s);
    if (!s.empty()) sodium_memzero(s.data(), s.size());
    std::vector<unsigned char> out(norm.begin(), norm.end());
    if (!norm.empty()) sodium_memzero(norm.data(), norm.size());
    return out;
}

static bool confirm_yesno(const std::string& prompt) {
    std::cerr << prompt;
    std::string ans;
    std::getline(std::cin, ans);

    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!ans.empty() && is_space(static_cast<unsigned char>(ans.front()))) ans.erase(ans.begin());
    while (!ans.empty() && is_space(static_cast<unsigned char>(ans.back()))) ans.pop_back();
    for (auto& c : ans) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return (ans == "yes" || ans == "y");
}

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
If you pass only positional args (no options starting with '-'):

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

10) Exchange (-e)
-----------------
Step 1:
  cyph -e -k? -o mykey

Step 2:
  cyph -e mykey.cyphkey -k?
)";

static void usage_help() {
    std::cerr <<
R"(cyph - streaming file encryption tool (.cyph) + wrapped keyfiles (.cyphkey)

USAGE:
  cyph --version
  cyph --help
  cyph -man
  cyph -manprint
  cyph -gen [-o <path>]
  cyph -e -k <...> -o <name_or_path>
  cyph -e <file.cyphkey> -k <...>

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

CREATE .cyphkey:
  cyph -key <keyfile> -o <name_or_path> (-K <file> | -K=<text> | -K?) [-level N]

OPTIONS:
  -level N   : 0=interactive, 1=moderate, 2=sensitive
  -anon      : encrypt only
  -WIPE      : after SUCCESS delete originals (encrypt) and/or on-disk key file (-k <file>)

Run: cyph -man
)";
}

static void usage_man() {
    std::cerr << MANUAL_TEXT;
}

static void write_manual_file() {
    std::ofstream f(MANUAL_FILE, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error(std::string("Cannot open for writing: ") + MANUAL_FILE);
    f.write(MANUAL_TEXT, static_cast<std::streamsize>(std::strlen(MANUAL_TEXT)));
    if (!f) throw std::runtime_error(std::string("Failed to write: ") + MANUAL_FILE);
}

enum class Mode { None, Encrypt, Decrypt, Gen, KeyWrap, Exchange };
enum class KeySrc { None, File, Inline, Prompt };

struct KeySpec {
    KeySrc src = KeySrc::None;
    std::string path;
    std::string inline_s;
    bool is_set() const { return src != KeySrc::None; }
    bool is_file() const { return src == KeySrc::File; }
};

struct Args {
    bool show_help = false;
    bool show_version = false;
    bool show_man = false;
    bool man_print = false;

    Mode mode = Mode::None;

    std::vector<std::string> enc_inputs;
    std::vector<std::string> dec_inputs;

    std::string out;
    bool restore_name = false;
    bool show_stdout = false;
    bool wipe = false;
    bool anon = false;
    int level = 0;

    KeySpec key;
    KeySpec master;

    bool gen = false;

    bool keywrap = false;
    std::string keywrap_input_keyfile;

    bool exchange = false;
    std::string exchange_keyfile;
};

static bool all_positional_no_directives(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (looks_like_option(a)) return false;
    }
    return argc >= 2;
}

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

    if (all_positional_no_directives(argc, argv)) {
        if (argc == 2) {
            std::string f1 = argv[1];
            if (has_ext(f1, cyph::EXT_CYPH)) {
                a.mode = Mode::Decrypt;
                a.dec_inputs = {f1};
                a.key.src = KeySrc::Prompt;
                a.restore_name = true;
                return a;
            }
            a.mode = Mode::Encrypt;
            a.enc_inputs = {f1};
            a.key.src = KeySrc::Prompt;
            return a;
        }

        if (argc == 3) {
            std::string p1 = argv[1];
            std::string p2 = argv[2];

            const bool p1_cyph = has_ext(p1, cyph::EXT_CYPH);
            const bool p2_cyph = has_ext(p2, cyph::EXT_CYPH);

            if (p1_cyph ^ p2_cyph) {
                std::string cyphf = p1_cyph ? p1 : p2;
                std::string keyf = p1_cyph ? p2 : p1;

                a.mode = Mode::Decrypt;
                a.dec_inputs = {cyphf};
                a.key.src = KeySrc::File;
                a.key.path = keyf;
                a.restore_name = true;

                if (has_ext(keyf, cyph::EXT_CYPHKEY)) {
                    a.master.src = KeySrc::Prompt;
                }
                return a;
            }

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
        } else if (arg == "-e") {
            a.exchange = true;
            a.mode = Mode::Exchange;
            if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
                a.exchange_keyfile = std::string(argv[++i]);
            }
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

    if (a.mode == Mode::Exchange) {
        if (!a.key.is_set()) throw std::runtime_error("Key required for -e: -k <file> / -k=<text> / -k?");
        if (a.exchange_keyfile.empty()) {
            if (a.out.empty()) throw std::runtime_error("Missing -o <name_or_path> for -e (step1)");
        } else {
            if (!a.out.empty()) throw std::runtime_error("-o is not used with -e <file> (step2)");
        }
        return a;
    }

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

    if (a.key.is_file() && has_ext(a.key.path, cyph::EXT_CYPHKEY) && !a.master.is_set()) {
        throw std::runtime_error("Using -k <file.cyphkey> requires master key: -K <file> / -K=<text> / -K?");
    }

    return a;
}

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

static std::vector<unsigned char> key_from_inline_text_normalized(const std::string& inline_key) {
    std::string norm = lowercase_no_ws(inline_key);
    std::vector<unsigned char> out(norm.begin(), norm.end());
    if (!norm.empty()) sodium_memzero(norm.data(), norm.size());
    return out;
}

static std::vector<unsigned char> material_from_keyspec_normalized(const KeySpec& ks,
                                                                   const std::string& prompt) {
    if (ks.src == KeySrc::File) return key_from_file(ks.path);
    if (ks.src == KeySrc::Inline) return key_from_inline_text_normalized(ks.inline_s);
    if (ks.src == KeySrc::Prompt) return prompt_key_normalized(prompt);
    throw std::runtime_error("Key not provided");
}

static std::vector<unsigned char> resolve_main_key_material(const Args& args) {
    if (args.key.src == KeySrc::File && has_ext(args.key.path, cyph::EXT_CYPHKEY)) {
        std::vector<unsigned char> master_mat =
            material_from_keyspec_normalized(args.master, "Enter master key (-K?): ");
        std::vector<unsigned char> wrapped_plain = cyph::decrypt_payload_to_bytes(args.key.path, master_mat);
        secure_wipe(master_mat);
        return wrapped_plain;
    }
    return material_from_keyspec_normalized(args.key, "Enter key (-k?): ");
}

static std::vector<unsigned char> resolve_master_key_material(const Args& args) {
    return material_from_keyspec_normalized(args.master, "Enter master key (-K?): ");
}

static std::string make_encrypt_out_path(const std::string& in,
                                         const Args& args,
                                         bool multiple) {
    if (args.out.empty()) return cyph::ensure_cyph_ext(in);

    fs::path outp(args.out);
    if (multiple) {
        ensure_dir_exists(outp);
        std::string name = cyph::ensure_cyph_ext(fs::path(in).filename().string());
        return (outp / name).string();
    }

    fs::path parent = outp.parent_path();
    if (!parent.empty()) ensure_dir_exists(parent);
    return cyph::ensure_cyph_ext(args.out);
}

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

static std::string gen_key_text(std::size_t len = 64) {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static constexpr std::size_t A = sizeof(alphabet) - 1;

    std::string out;
    out.resize(len);

    std::vector<unsigned char> rnd(len);
    randombytes_buf(rnd.data(), rnd.size());

    for (std::size_t i = 0; i < len; i++) out[i] = alphabet[rnd[i] % A];
    secure_wipe(rnd);
    return out;
}

int main(int argc, char** argv) {
    if (sodium_init() < 0) {
        std::cerr << "sodium_init failed\n";
        return 1;
    }

    try {
        Args args = parse_args(argc, argv);

        if (args.show_version) {
            std::cout << "cyph " << cyph::VERSION << "\n";
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

        const cyph::KdfParams kdf = cyph::kdf_params_for_level(args.level);

        if (args.mode == Mode::Exchange) {
            std::vector<unsigned char> pw_mat = material_from_keyspec_normalized(args.key, "Enter key (-k?): ");
            if (pw_mat.empty()) throw std::runtime_error("Key material is empty (after normalization)");

            if (args.exchange_keyfile.empty()) {
                unsigned char pk[crypto_kx_PUBLICKEYBYTES];
                unsigned char sk[crypto_kx_SECRETKEYBYTES];
                crypto_kx_keypair(pk, sk);

                std::vector<unsigned char> sk_bytes(sk, sk + crypto_kx_SECRETKEYBYTES);
                sodium_memzero(sk, sizeof(sk));

                cyph::create_cyphkey_from_bytes(sk_bytes, args.out, pw_mat, kdf, "ex_priv_v1");
                secure_wipe(sk_bytes);

                const std::string pub_text = cyph::pubkey_to_text_cyphx1(pk);
                const std::string fp = cyph::fingerprint6_from_pubkey_bytes(pk, crypto_kx_PUBLICKEYBYTES);

                std::cout << "Exchange keyfile created: " << cyph::ensure_cyphkey_ext(args.out) << "\n";
                std::cout << "Public key:\n" << pub_text << "\n";
                std::cout << "Fingerprint (6 words): " << fp << "\n";
                std::cout << "Now run on the same machine:\n";
                std::cout << "  cyph -e " << cyph::ensure_cyphkey_ext(args.out) << " -k?\n";

                sodium_memzero(pk, sizeof(pk));
                secure_wipe(pw_mat);
                return 0;
            } else {
                const std::string keyfile_path = cyph::ensure_cyphkey_ext(args.exchange_keyfile);

                std::vector<unsigned char> sk_bytes = cyph::decrypt_payload_to_bytes(keyfile_path, pw_mat);
                if (sk_bytes.size() != crypto_kx_SECRETKEYBYTES) {
                    secure_wipe(sk_bytes);
                    secure_wipe(pw_mat);
                    throw std::runtime_error("Exchange file does not contain a valid private key (unexpected payload length)");
                }

                unsigned char sk[crypto_kx_SECRETKEYBYTES];
                std::memcpy(sk, sk_bytes.data(), crypto_kx_SECRETKEYBYTES);
                secure_wipe(sk_bytes);

                unsigned char my_pk[crypto_kx_PUBLICKEYBYTES];
                crypto_scalarmult_curve25519_base(my_pk, sk);

                std::string peer_pub_line = prompt_line("Enter peer public key (cyphx1:...): ");
                std::string peer_pub_ws = strip_ws(peer_pub_line);
                if (!peer_pub_line.empty()) sodium_memzero(peer_pub_line.data(), peer_pub_line.size());

                std::vector<unsigned char> peer_pk_vec = cyph::pubkey_from_text_cyphx1(peer_pub_ws);
                if (!peer_pub_ws.empty()) sodium_memzero(peer_pub_ws.data(), peer_pub_ws.size());

                unsigned char peer_pk[crypto_kx_PUBLICKEYBYTES];
                std::memcpy(peer_pk, peer_pk_vec.data(), crypto_kx_PUBLICKEYBYTES);
                secure_wipe(peer_pk_vec);

                const std::string fp = cyph::fingerprint6_from_pubkey_bytes(peer_pk, crypto_kx_PUBLICKEYBYTES);
                std::cerr << "Peer fingerprint (6 words): " << fp << "\n";
                if (!confirm_yesno("Does the fingerprint match (Yes/No)? ")) {
                    sodium_memzero(sk, sizeof(sk));
                    sodium_memzero(my_pk, sizeof(my_pk));
                    sodium_memzero(peer_pk, sizeof(peer_pk));
                    secure_wipe(pw_mat);
                    throw std::runtime_error("Fingerprint not confirmed by user");
                }

                unsigned char raw_shared[crypto_scalarmult_BYTES];
                if (crypto_scalarmult_curve25519(raw_shared, sk, peer_pk) != 0) {
                    sodium_memzero(sk, sizeof(sk));
                    sodium_memzero(my_pk, sizeof(my_pk));
                    sodium_memzero(peer_pk, sizeof(peer_pk));
                    sodium_memzero(raw_shared, sizeof(raw_shared));
                    secure_wipe(pw_mat);
                    throw std::runtime_error("Key exchange failed (crypto_scalarmult_curve25519)");
                }

                std::vector<unsigned char> shared_key = cyph::derive_shared_key_v1(my_pk, peer_pk, raw_shared);
                sodium_memzero(raw_shared, sizeof(raw_shared));

                cyph::create_cyphkey_from_bytes(shared_key, keyfile_path, pw_mat, kdf, "ex_shared_v1");

                std::cout << "Shared key stored in: " << keyfile_path << "\n";

                secure_wipe(shared_key);
                sodium_memzero(sk, sizeof(sk));
                sodium_memzero(my_pk, sizeof(my_pk));
                sodium_memzero(peer_pk, sizeof(peer_pk));
                secure_wipe(pw_mat);
                return 0;
            }
        }

        if (args.mode == Mode::KeyWrap) {
            std::vector<unsigned char> master = resolve_master_key_material(args);
            cyph::create_cyphkey_file(args.keywrap_input_keyfile, args.out, master, kdf);
            secure_wipe(master);
            std::cout << "Created wrapped key file: " << cyph::ensure_cyphkey_ext(args.out) << "\n";
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
            std::uint8_t flags = 0;
            if (args.anon) flags |= cyph::FLAG_ANON_META;

            for (const auto& in : args.enc_inputs) {
                const std::string outp = make_encrypt_out_path(in, args, multiple);
                const std::string meta_name = args.anon ? std::string("anonymous") : basename_only(in);
                cyph::encrypt_file_stream(in, outp, key_material, kdf, flags, meta_name);
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
                std::string restored = cyph::read_restored_name_quick(enc_in, key_material);

                const std::string outp = make_decrypt_out_path(enc_in, args, multiple, restored);
                const bool write_file = !outp.empty();
                const bool to_stdout = args.show_stdout;

                cyph::decrypt_file_stream(enc_in, outp, key_material, to_stdout, write_file);

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
