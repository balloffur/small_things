// cyph.cpp (streaming version + updated CLI + -WIPE + auto mkdir)
//
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
// Usage (new API):
//   cyph --version
//   cyph -h | -help | --help
//
//   Encrypt:
//     cyph -f /abs/in1 [/abs/in2 ...] -k keys.txt [-o /abs/outfile_or_outdir] [-WIPE]
//   Decrypt:
//     cyph -i /abs/in1.mycyph [/abs/in2.mycyph ...] -k keys.txt [-o /abs/outfile_or_outdir] [-d] [-s] [-WIPE]
//
// Key:
//   -k <file>     : if ends with .txt => normalize (remove whitespace + lowercase), else raw bytes
//   -k=<text>     : inline text key (same normalization as .txt)
//
// -o behavior:
//   * Single input: -o is a file path (encryption auto-adds .mycyph if missing)
//   * Multiple inputs: -o is treated as a directory; it will be created if missing
//
// -d behavior:
//   Decrypt using original stored file name (basename) from encrypted file.
//   Works with -s and/or -o.
//
// -WIPE behavior (with confirmation Yes/No):
//   After SUCCESS:
//     * Encrypt: deletes ORIGINAL input files (-f ...) AND deletes key file (-k <file>)
//     * Decrypt: deletes key file (-k <file>)
//   (If using -k=<text>, there is no key file to delete; inline key is wiped from memory best-effort)

#include <sodium.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static constexpr const char* VERSION = "2.1.0";

// ------------------------- helpers -------------------------

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

    std::vector<unsigned char> buf(static_cast<size_t>(n));
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
        fs::create_directories(parent, ec); // best-effort
        if (ec) throw std::runtime_error("Failed to create directory: " + parent.string());
    }

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("Cannot open for writing: " + path);

    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    if (!f) throw std::runtime_error("Failed to write: " + path);
}

static void write_stdout(const unsigned char* p, size_t n) {
    std::cout.write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(n));
    if (!std::cout) throw std::runtime_error("Failed to write to stdout");
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

    // Trim spaces
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!ans.empty() && is_space(ans.front())) ans.erase(ans.begin());
    while (!ans.empty() && is_space(ans.back())) ans.pop_back();

    // Lowercase
    for (auto& c : ans) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    return (ans == "yes" || ans == "y");
}

static std::string ensure_mycyph_ext(std::string out) {
    if (!has_ext(out, ".mycyph")) out += ".mycyph";
    return out;
}

static std::string basename_only(const std::string& p) {
    return fs::path(p).filename().string();
}

// ------------------------- format + KDF -------------------------

// MAGIC (8) + SALT (16) + secretstream header (24) + [len|frame]...
static const unsigned char MAGIC[8] = {'M','Y','C','Y','P','H','S','2'};

static constexpr size_t SALT_LEN = crypto_pwhash_SALTBYTES; // 16
static constexpr unsigned long long OPSLIMIT = crypto_pwhash_OPSLIMIT_INTERACTIVE;
static constexpr size_t MEMLIMIT = crypto_pwhash_MEMLIMIT_INTERACTIVE;

static void derive_key(unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES],
                       const unsigned char salt[SALT_LEN],
                       const std::vector<unsigned char>& key_material) {
    if (key_material.empty()) throw std::runtime_error("Key material is empty");

    const char* pw = reinterpret_cast<const char*>(key_material.data());
    const size_t pwlen = key_material.size();

    if (crypto_pwhash(
            key, crypto_secretstream_xchacha20poly1305_KEYBYTES,
            pw, pwlen,
            salt,
            OPSLIMIT, MEMLIMIT,
            crypto_pwhash_ALG_DEFAULT
        ) != 0) {
        throw std::runtime_error("crypto_pwhash failed (OOM?)");
    }
}

static std::vector<unsigned char> key_from_file(const std::string& key_path) {
    auto data = read_file_bytes(key_path);

    if (has_ext(key_path, ".txt")) {
        std::string text(reinterpret_cast<const char*>(data.data()), data.size());
        std::string norm = lowercase_no_ws(text);
        if (!text.empty()) sodium_memzero(text.data(), text.size());

        std::vector<unsigned char> out(norm.begin(), norm.end());
        if (!norm.empty()) sodium_memzero(norm.data(), norm.size());
        return out;
    }
    return data; // binary raw
}

static std::vector<unsigned char> key_from_inline_text(const std::string& inline_key) {
    std::string norm = lowercase_no_ws(inline_key);
    std::vector<unsigned char> out(norm.begin(), norm.end());
    if (!norm.empty()) sodium_memzero(norm.data(), norm.size());
    return out;
}

// ------------------------- CLI -------------------------

struct Args {
    bool show_help = false;
    bool show_version = false;

    bool gen = false;

    std::vector<std::string> enc_inputs; // -f ...
    std::vector<std::string> dec_inputs; // -i ...

    std::string out;           // -o
    bool restore_name = false; // -d
    bool show_stdout = false;  // -s
    bool wipe = false;         // -WIPE

    bool has_key_file = false;
    std::string key_file;      // -k <path>

    bool has_inline_key = false;
    std::string inline_key;    // -k=<text>
};

static void usage_detailed() {
    std::cerr <<
R"(cyph - streaming file encryption tool (.mycyph)

USAGE:
  cyph --version
  cyph -h | -help | --help

ENCRYPT:
  cyph -f <abs_path1> [abs_path2 ...] -k <keyfile> [-o <out_file_or_out_dir>] [-WIPE]

DECRYPT:
  cyph -i <abs_path1> [abs_path2 ...] -k <keyfile> [-o <out_file_or_out_dir>] [-d] [-s] [-WIPE]

KEY:
  -k <file>     : if .txt => lowercase + remove whitespace, else raw bytes
  -k=<text>     : inline key text (same normalization as .txt)

OPTIONS:
  -f <paths...>   Encrypt file(s)
  -i <paths...>   Decrypt file(s)
  -o <path>       If single input: output file path
                  If multiple inputs: output directory (created if missing)
                  Encrypt: ".mycyph" auto-added if missing.
  -d              Decrypt to original stored filename (basename) from encrypted file
  -s              Also print decrypted bytes to stdout
  -gen            Generate random text key and print to stdout (or save with -o)
  -WIPE           With confirmation (Yes/No), after SUCCESS:
                    Encrypt: delete original input files (-f ...) AND delete key file (-k <file>)
                    Decrypt: delete key file (-k <file>)

NOTES:
  - The encrypted file stores the original filename as the FIRST ENCRYPTED FRAME.
  - Streaming mode: works with huge files; no full-file buffering.
  - Secure deletion is not guaranteed on modern filesystems/flash; -WIPE means "delete".

EXAMPLES:
  cyph -f /abs/note.txt -k /abs/key.txt
  cyph -f /abs/a.txt /abs/b.bin -k /abs/key.txt -o /abs/outdir
  cyph -i /abs/note.txt.mycyph -k /abs/key.txt -d
  cyph -i /abs/note.txt.mycyph -k /abs/key.txt -s
  cyph -i /abs/note.txt.mycyph -k /abs/key.txt -d -s -o /abs/outdir
  cyph -gen
  cyph -gen -o /abs/key.txt
)";
}

static Args parse_args(int argc, char** argv) {
    Args a;

    auto need = [&](int& i, const std::string& opt) -> std::string {
        if (i + 1 >= argc) throw std::runtime_error("Missing value for " + opt);
        return std::string(argv[++i]);
    };

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--version") {
            a.show_version = true;
        } else if (arg == "-h" || arg == "-help" || arg == "--help") {
            a.show_help = true;
        } else if (arg == "-gen") {
            a.gen = true;
        } else if (arg == "-s") {
            a.show_stdout = true;
        } else if (arg == "-d") {
            a.restore_name = true;
        } else if (arg == "-WIPE") {
            a.wipe = true;
        } else if (arg == "-o") {
            a.out = need(i, "-o");
        } else if (arg == "-k") {
            a.key_file = need(i, "-k");
            a.has_key_file = true;
        } else if (arg.rfind("-k=", 0) == 0) {
            a.inline_key = arg.substr(3);
            a.has_inline_key = true;
        } else if (arg == "-f") {
            while (i + 1 < argc && !looks_like_option(argv[i + 1])) {
                a.enc_inputs.emplace_back(argv[++i]);
            }
            if (a.enc_inputs.empty()) throw std::runtime_error("No paths after -f");
        } else if (arg == "-i") {
            while (i + 1 < argc && !looks_like_option(argv[i + 1])) {
                a.dec_inputs.emplace_back(argv[++i]);
            }
            if (a.dec_inputs.empty()) throw std::runtime_error("No paths after -i");
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (a.show_help || a.show_version) return a;

    if (a.gen) {
        // no key needed
        return a;
    }

    const bool enc = !a.enc_inputs.empty();
    const bool dec = !a.dec_inputs.empty();
    if (enc == dec) {
        throw std::runtime_error("Choose exactly one mode: -f (encrypt) OR -i (decrypt)");
    }
    if (!(a.has_key_file || a.has_inline_key)) {
        throw std::runtime_error("Key required: -k <file> or -k=<text>");
    }
    if (a.has_key_file && a.has_inline_key) {
        throw std::runtime_error("Use either -k <file> OR -k=<text>, not both");
    }

    return a;
}

// ------------------------- streaming crypto -------------------------

static constexpr size_t CHUNK = 1u << 20; // 1 MiB

static void ensure_dir_exists(const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) throw std::runtime_error("Failed to create directory: " + dir.string());
}

static std::string make_encrypt_out_path(const std::string& in,
                                        const Args& args,
                                        bool multiple) {
    if (args.out.empty()) {
        return ensure_mycyph_ext(in);
    }

    fs::path outp(args.out);

    if (multiple) {
        // treat -o as directory, create it if missing
        ensure_dir_exists(outp);
        std::string name = fs::path(in).filename().string();
        name = ensure_mycyph_ext(name);
        return (outp / name).string();
    }

    // single input: -o is file path
    fs::path parent = outp.parent_path();
    if (!parent.empty()) ensure_dir_exists(parent);
    return ensure_mycyph_ext(args.out);
}

static std::string make_decrypt_out_path(const std::string& enc_in,
                                        const Args& args,
                                        bool multiple,
                                        const std::string& restored_name) {
    if (!args.out.empty()) {
        fs::path outp(args.out);
        if (multiple) {
            // treat -o as directory, create it if missing
            ensure_dir_exists(outp);
            fs::path fname = args.restore_name
                                 ? fs::path(restored_name)
                                 : fs::path(enc_in).filename().replace_extension(""); // best-effort
            return (outp / fname).string();
        }

        // single input: -o is file path
        fs::path parent = outp.parent_path();
        if (!parent.empty()) ensure_dir_exists(parent);
        return args.out;
    }

    if (args.restore_name) {
        fs::path dir = fs::path(enc_in).parent_path();
        if (!dir.empty()) ensure_dir_exists(dir); // generally exists, best-effort
        return (dir / restored_name).string();
    }

    // default: no file output unless -s
    return "";
}

static void encrypt_file_stream(const std::string& in_path,
                                const std::string& out_path,
                                const std::vector<unsigned char>& key_material) {
    std::ifstream in(in_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + in_path);

    fs::path outp(out_path);
    if (!outp.parent_path().empty()) ensure_dir_exists(outp.parent_path());

    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open output: " + out_path);

    unsigned char salt[SALT_LEN];
    randombytes_buf(salt, sizeof salt);

    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    derive_key(key, salt, key_material);

    crypto_secretstream_xchacha20poly1305_state st;
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    if (crypto_secretstream_xchacha20poly1305_init_push(&st, header, key) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("secretstream init_push failed");
    }

    // Write container header
    out.write(reinterpret_cast<const char*>(MAGIC), sizeof(MAGIC));
    out.write(reinterpret_cast<const char*>(salt), sizeof(salt));
    out.write(reinterpret_cast<const char*>(header), sizeof(header));
    if (!out) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Failed to write header: " + out_path);
    }

    // First encrypted frame: original filename (basename)
    const std::string fname = basename_only(in_path);
    std::vector<unsigned char> meta(fname.begin(), fname.end());
    std::vector<unsigned char> meta_ct(meta.size() + crypto_secretstream_xchacha20poly1305_ABYTES);

    unsigned long long meta_ct_len = 0;
    if (crypto_secretstream_xchacha20poly1305_push(
            &st,
            meta_ct.data(), &meta_ct_len,
            meta.data(), meta.size(),
            nullptr, 0,
            crypto_secretstream_xchacha20poly1305_TAG_MESSAGE
        ) != 0) {
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

    // Data frames
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
                &st,
                ct.data(), &ct_len,
                buf.data(), static_cast<size_t>(got),
                nullptr, 0,
                tag
            ) != 0) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("secretstream push(data) failed");
        }

        uint32_t len32 = static_cast<uint32_t>(ct_len);
        out.write(reinterpret_cast<const char*>(&len32), sizeof(len32));
        out.write(reinterpret_cast<const char*>(ct.data()),
                  static_cast<std::streamsize>(ct_len));
        if (!out) {
            sodium_memzero(key, sizeof key);
            throw std::runtime_error("Failed to write ciphertext frame");
        }

        if (eof) break;
    }

    sodium_memzero(key, sizeof key);
}

static std::string read_restored_name_quick(const std::string& enc_in,
                                            const std::vector<unsigned char>& key_material) {
    std::ifstream in(enc_in, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + enc_in);

    unsigned char magic[sizeof(MAGIC)];
    unsigned char salt[SALT_LEN];
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];

    in.read(reinterpret_cast<char*>(magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(salt), sizeof(salt));
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    if (!in) throw std::runtime_error("Invalid/corrupt file header: " + enc_in);

    if (sodium_memcmp(magic, MAGIC, sizeof(MAGIC)) != 0) {
        throw std::runtime_error("Bad magic/version: " + enc_in);
    }

    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    derive_key(key, salt, key_material);

    crypto_secretstream_xchacha20poly1305_state st;
    if (crypto_secretstream_xchacha20poly1305_init_pull(&st, header, key) != 0) {
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
            &st,
            meta_pt.data(), &meta_pt_len,
            &tag,
            meta_ct.data(), meta_ct.size(),
            nullptr, 0
        ) != 0) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Wrong key or corrupt file (meta)");
    }

    meta_pt.resize(static_cast<size_t>(meta_pt_len));
    std::string restored(reinterpret_cast<const char*>(meta_pt.data()), meta_pt.size());

    sodium_memzero(key, sizeof key);
    return restored;
}

static void decrypt_file_stream(const std::string& enc_path,
                                const std::string& out_path_or_empty,
                                const std::vector<unsigned char>& key_material,
                                bool to_stdout,
                                bool write_file) {
    std::ifstream in(enc_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + enc_path);

    unsigned char magic[sizeof(MAGIC)];
    unsigned char salt[SALT_LEN];
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];

    in.read(reinterpret_cast<char*>(magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(salt), sizeof(salt));
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    if (!in) throw std::runtime_error("Invalid/corrupt file header: " + enc_path);

    if (sodium_memcmp(magic, MAGIC, sizeof(MAGIC)) != 0) {
        throw std::runtime_error("Bad magic/version: " + enc_path);
    }

    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    derive_key(key, salt, key_material);

    crypto_secretstream_xchacha20poly1305_state st;
    if (crypto_secretstream_xchacha20poly1305_init_pull(&st, header, key) != 0) {
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

    // Meta frame (filename) - consume but ignore here (caller already used quick read if needed)
    std::vector<unsigned char> meta_ct;
    if (!read_frame(meta_ct)) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Missing meta frame");
    }

    std::vector<unsigned char> meta_pt(meta_ct.size());
    unsigned long long meta_pt_len = 0;
    unsigned char meta_tag = 0;
    if (crypto_secretstream_xchacha20poly1305_pull(
            &st,
            meta_pt.data(), &meta_pt_len,
            &meta_tag,
            meta_ct.data(), meta_ct.size(),
            nullptr, 0
        ) != 0) {
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
                nullptr, 0
            ) != 0) {
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

        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
            done = true;
        }
    }

    if (!done) {
        sodium_memzero(key, sizeof key);
        throw std::runtime_error("Corrupt file: missing FINAL tag");
    }

    sodium_memzero(key, sizeof key);
}

// ------------------------- -gen -------------------------

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

// ------------------------- main -------------------------

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
            usage_detailed();
            return 0;
        }

        if (args.gen) {
            const std::string key = gen_key_text(64);
            if (!args.out.empty()) {
                write_file_bytes(args.out,
                                 std::vector<unsigned char>(key.begin(), key.end()));
                std::cout << "Generated key saved to: " << args.out << "\n";
            } else {
                std::cout << key << "\n";
            }
            return 0;
        }

        // Load key material
        std::vector<unsigned char> key_material;
        if (args.has_key_file) key_material = key_from_file(args.key_file);
        else key_material = key_from_inline_text(args.inline_key);

        if (key_material.empty()) throw std::runtime_error("Key material is empty (after normalization)");

        const bool encrypt_mode = !args.enc_inputs.empty();
        const bool multiple = encrypt_mode ? (args.enc_inputs.size() > 1)
                                           : (args.dec_inputs.size() > 1);

        // Precompute wipe targets & ask once (as requested: "говорит обо всем этом и спрашивает!")
        bool do_wipe = false;
        std::vector<std::string> wipe_targets;
        if (args.wipe) {
            if (encrypt_mode) {
                // Encrypt: delete originals + keyfile
                wipe_targets = args.enc_inputs;
                if (args.has_key_file) wipe_targets.push_back(args.key_file);
            } else {
                // Decrypt: delete keyfile
                if (args.has_key_file) wipe_targets.push_back(args.key_file);
            }

            if (wipe_targets.empty()) {
                std::cerr << "Note: -WIPE requested, but there are no on-disk key files to delete.\n";
                do_wipe = false;
            } else {
                do_wipe = confirm_wipe(wipe_targets);
                if (!do_wipe) std::cerr << "WIPE cancelled by user.\n";
            }
        }

        if (encrypt_mode) {
            for (const auto& in : args.enc_inputs) {
                const std::string outp = make_encrypt_out_path(in, args, multiple);
                encrypt_file_stream(in, outp, key_material);
                std::cout << "Encrypted: " << outp << "\n";
            }

            if (do_wipe) {
                bool any_fail = false;
                // remove originals
                for (const auto& in : args.enc_inputs) {
                    if (!remove_file_best_effort(in)) {
                        any_fail = true;
                        std::cerr << "Failed to delete: " << in << "\n";
                    } else {
                        std::cout << "Deleted: " << in << "\n";
                    }
                }
                // remove key file
                if (args.has_key_file) {
                    if (!remove_file_best_effort(args.key_file)) {
                        any_fail = true;
                        std::cerr << "Failed to delete key file: " << args.key_file << "\n";
                    } else {
                        std::cout << "Deleted key file: " << args.key_file << "\n";
                    }
                }
                if (any_fail) {
                    std::cerr << "WIPE completed with errors.\n";
                } else {
                    std::cout << "WIPE completed.\n";
                }
            }

        } else {
            bool any_decrypt_fail = false;

            for (const auto& enc_in : args.dec_inputs) {
                // Need restored name for -d or multi-outdir mapping
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
                if (args.has_key_file) {
                    if (!remove_file_best_effort(args.key_file)) {
                        any_decrypt_fail = true;
                        std::cerr << "Failed to delete key file: " << args.key_file << "\n";
                    } else {
                        std::cout << "Deleted key file: " << args.key_file << "\n";
                    }
                }
                if (any_decrypt_fail) std::cerr << "WIPE completed with errors.\n";
                else std::cout << "WIPE completed.\n";
            }
        }

        // Best-effort wipe key material
        secure_wipe(key_material);
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        usage_detailed();
        return 2;
    }
}
