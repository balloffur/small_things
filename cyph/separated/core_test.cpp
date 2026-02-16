// core_test.cpp
#include "core.h"
#include <sodium.h>

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_fail = 0;

#define EXPECT_TRUE(x)                                                                     \
    do {                                                                                   \
        if (!(x)) {                                                                        \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " EXPECT_TRUE(" #x    \
                      << ")\n";                                                            \
            ++g_fail;                                                                      \
        }                                                                                  \
    } while (0)

#define EXPECT_EQ(a, b)                                                                    \
    do {                                                                                   \
        auto _a = (a);                                                                     \
        auto _b = (b);                                                                     \
        if (!((_a) == (_b))) {                                                             \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " EXPECT_EQ(" #a      \
                      << ", " #b << ") got (" << _a << ") vs (" << _b << ")\n";            \
            ++g_fail;                                                                      \
        }                                                                                  \
    } while (0)

#define EXPECT_THROW(stmt)                                                                 \
    do {                                                                                   \
        bool _thrown = false;                                                              \
        try {                                                                              \
            (void)(stmt);                                                                  \
        } catch (const std::exception&) {                                                  \
            _thrown = true;                                                                \
        }                                                                                  \
        if (!_thrown) {                                                                    \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " EXPECT_THROW("      \
                      << #stmt << ")\n";                                                   \
            ++g_fail;                                                                      \
        }                                                                                  \
    } while (0)

static void expect_bytes_eq(const std::vector<unsigned char>& a,
                            const std::vector<unsigned char>& b,
                            const char* what) {
    if (a.size() != b.size() || (a.size() && std::memcmp(a.data(), b.data(), a.size()) != 0)) {
        std::cerr << "[FAIL] " << what << " bytes mismatch: " << a.size() << " vs " << b.size()
                  << "\n";
        ++g_fail;
    }
}

static std::vector<unsigned char> read_all(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("read_all: cannot open " + p.string());
    f.seekg(0, std::ios::end);
    std::streamoff n = f.tellg();
    f.seekg(0, std::ios::beg);
    if (n < 0) throw std::runtime_error("read_all: stat failed " + p.string());
    std::vector<unsigned char> out(static_cast<size_t>(n));
    if (n > 0) {
        f.read(reinterpret_cast<char*>(out.data()), n);
        if (!f) throw std::runtime_error("read_all: read failed " + p.string());
    }
    return out;
}

static void write_all(const fs::path& p, const std::vector<unsigned char>& data) {
    if (!p.parent_path().empty()) fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("write_all: cannot open " + p.string());
    if (!data.empty()) {
        f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!f) throw std::runtime_error("write_all: write failed " + p.string());
    }
}

static std::vector<unsigned char> rand_bytes(size_t n) {
    std::vector<unsigned char> v(n);
    randombytes_buf(v.data(), v.size());
    return v;
}

static std::vector<unsigned char> norm_key(const std::string& s) {
    // core doesn't expose normalization. Tests mimic old behavior: lowercase + strip whitespace
    std::string out;
    out.reserve(s.size());
    for (unsigned char ch : s) {
        if (std::isspace(ch)) continue;
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return std::vector<unsigned char>(out.begin(), out.end());
}

static fs::path tmp_dir() {
    fs::path base = fs::temp_directory_path();
    auto r = rand_bytes(8);
    char name[64];
    std::snprintf(name, sizeof(name), "cyph_core_test_%02x%02x%02x%02x%02x%02x%02x%02x",
                  r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
    fs::path d = base / name;
    fs::create_directories(d);
    return d;
}

static std::string basename_only(const fs::path& p) { return p.filename().string(); }

static void test_ext_helpers() {
    EXPECT_EQ(cyph::ensure_cyph_ext("a"), std::string("a.cyph"));
    EXPECT_EQ(cyph::ensure_cyph_ext("a.cyph"), std::string("a.cyph"));
    EXPECT_EQ(cyph::ensure_cyphkey_ext("k"), std::string("k.cyphkey"));
    EXPECT_EQ(cyph::ensure_cyphkey_ext("k.cyphkey"), std::string("k.cyphkey"));
}

static void test_roundtrip_small() {
    fs::path d = tmp_dir();
    fs::path in = d / "hello.bin";
    fs::path out = d / "hello.bin.cyph";
    fs::path dec = d / "hello_out.bin";

    auto plain = rand_bytes(1234);
    write_all(in, plain);

    auto key = norm_key("My Key 123");
    auto kdf = cyph::kdf_params_for_level(0);

    cyph::encrypt_file_stream(in.string(), out.string(), key, kdf, 0, basename_only(in));
    std::string restored = cyph::read_restored_name_quick(out.string(), key);
    EXPECT_EQ(restored, basename_only(in));

    cyph::decrypt_file_stream(out.string(), dec.string(), key, false, true);
    auto got = read_all(dec);
    expect_bytes_eq(got, plain, "roundtrip_small");

    fs::remove_all(d);
}

static void test_roundtrip_large_multi_chunk() {
    fs::path d = tmp_dir();
    fs::path in = d / "big.bin";
    fs::path out = d / "big.bin.cyph";
    fs::path dec = d / "big_out.bin";

    // > 1MiB to force multiple frames (CHUNK = 1<<20 in core.cpp)
    auto plain = rand_bytes((1u << 20) + 54321);
    write_all(in, plain);

    auto key = norm_key("another key");
    auto kdf = cyph::kdf_params_for_level(1);

    cyph::encrypt_file_stream(in.string(), out.string(), key, kdf, 0, basename_only(in));
    cyph::decrypt_file_stream(out.string(), dec.string(), key, false, true);

    auto got = read_all(dec);
    expect_bytes_eq(got, plain, "roundtrip_large_multi_chunk");

    fs::remove_all(d);
}

static void test_empty_file() {
    fs::path d = tmp_dir();
    fs::path in = d / "empty.dat";
    fs::path out = d / "empty.dat.cyph";
    fs::path dec = d / "empty_out.dat";

    write_all(in, {});

    auto key = norm_key("abc DEF");
    auto kdf = cyph::kdf_params_for_level(2);

    cyph::encrypt_file_stream(in.string(), out.string(), key, kdf, 0, basename_only(in));
    cyph::decrypt_file_stream(out.string(), dec.string(), key, false, true);

    auto got = read_all(dec);
    EXPECT_TRUE(got.empty());

    fs::remove_all(d);
}

static void test_wrong_key_fails() {
    fs::path d = tmp_dir();
    fs::path in = d / "msg.txt";
    fs::path out = d / "msg.txt.cyph";
    fs::path dec = d / "msg_out.txt";

    std::vector<unsigned char> plain = {'h', 'e', 'l', 'l', 'o'};
    write_all(in, plain);

    auto key_ok = norm_key("correct horse battery staple");
    auto key_bad = norm_key("wrong key");
    auto kdf = cyph::kdf_params_for_level(0);

    cyph::encrypt_file_stream(in.string(), out.string(), key_ok, kdf, 0, basename_only(in));

    EXPECT_THROW(cyph::read_restored_name_quick(out.string(), key_bad));
    EXPECT_THROW(cyph::decrypt_file_stream(out.string(), dec.string(), key_bad, false, true));

    fs::remove_all(d);
}

static void test_cyphkey_wrap_unwrap_file() {
    fs::path d = tmp_dir();
    fs::path raw_keyfile = d / "rawkey.bin";
    fs::path wrapped = d / "wrapped.cyphkey";

    auto raw = rand_bytes(64);
    write_all(raw_keyfile, raw);

    auto master = norm_key("MASTER KEY");
    auto kdf = cyph::kdf_params_for_level(0);

    cyph::create_cyphkey_file(raw_keyfile.string(), wrapped.string(), master, kdf);

    auto unwrapped = cyph::decrypt_payload_to_bytes(wrapped.string(), master);
    expect_bytes_eq(unwrapped, raw, "cyphkey_wrap_unwrap_file");

    fs::remove_all(d);
}

static void test_create_cyphkey_from_bytes_roundtrip() {
    fs::path d = tmp_dir();
    fs::path wrapped = d / "memkey.cyphkey";

    auto plain = rand_bytes(48);
    auto pw = norm_key("pw material");
    auto kdf = cyph::kdf_params_for_level(1);

    cyph::create_cyphkey_from_bytes(plain, wrapped.string(), pw, kdf, "testmeta");
    auto got = cyph::decrypt_payload_to_bytes(wrapped.string(), pw);

    expect_bytes_eq(got, plain, "create_cyphkey_from_bytes_roundtrip");

    fs::remove_all(d);
}

static void test_pubkey_text_codec_and_fingerprint() {
    unsigned char pk[32];
    randombytes_buf(pk, sizeof(pk));

    std::string txt = cyph::pubkey_to_text_cyphx1(pk);
    EXPECT_TRUE(txt.rfind("cyphx1:", 0) == 0);

    auto decoded = cyph::pubkey_from_text_cyphx1(txt);
    EXPECT_EQ(decoded.size(), size_t(32));
    EXPECT_TRUE(std::memcmp(decoded.data(), pk, 32) == 0);

    std::string fp = cyph::fingerprint6_from_pubkey_bytes(pk, 32);
    // should be 6 words separated by 5 '-'
    int dash = 0;
    for (char c : fp) if (c == '-') dash++;
    EXPECT_EQ(dash, 5);
    EXPECT_TRUE(fp.size() > 6); // not empty

    EXPECT_THROW(cyph::pubkey_from_text_cyphx1("badprefix:AAAA"));
}

static void test_derive_shared_key_v1_symmetric_and_length() {
    unsigned char pk_a[crypto_kx_PUBLICKEYBYTES], sk_a[crypto_kx_SECRETKEYBYTES];
    unsigned char pk_b[crypto_kx_PUBLICKEYBYTES], sk_b[crypto_kx_SECRETKEYBYTES];

    crypto_kx_keypair(pk_a, sk_a);
    crypto_kx_keypair(pk_b, sk_b);

    unsigned char raw_ab[crypto_scalarmult_BYTES];
    unsigned char raw_ba[crypto_scalarmult_BYTES];

    // ВАЖНО: если вдруг вернуло !=0 — это ошибка теста, дальше сравнивать нельзя.
    if (crypto_scalarmult_curve25519(raw_ab, sk_a, pk_b) != 0) {
        throw std::runtime_error("crypto_scalarmult_curve25519(raw_ab) failed");
    }
    if (crypto_scalarmult_curve25519(raw_ba, sk_b, pk_a) != 0) {
        throw std::runtime_error("crypto_scalarmult_curve25519(raw_ba) failed");
    }

    // ECDH должен совпасть байт-в-байт
    if (std::memcmp(raw_ab, raw_ba, crypto_scalarmult_BYTES) != 0) {
        std::cerr << "[FAIL] ECDH raw mismatch\n";
        ++g_fail;
    }

    auto key1 = cyph::derive_shared_key_v1(pk_a, pk_b, raw_ab);
    auto key2 = cyph::derive_shared_key_v1(pk_b, pk_a, raw_ba);

    EXPECT_EQ(key1.size(), size_t(crypto_secretstream_xchacha20poly1305_KEYBYTES));
    EXPECT_EQ(key2.size(), size_t(crypto_secretstream_xchacha20poly1305_KEYBYTES));
    expect_bytes_eq(key1, key2, "derive_shared_key_v1_symmetric");

    sodium_memzero(sk_a, sizeof(sk_a));
    sodium_memzero(sk_b, sizeof(sk_b));
    sodium_memzero(raw_ab, sizeof(raw_ab));
    sodium_memzero(raw_ba, sizeof(raw_ba));
}


int main() {
    if (sodium_init() < 0) {
        std::cerr << "sodium_init failed\n";
        return 2;
    }

    try {
        std::cout << "[RUN] test_ext_helpers\n";
        test_ext_helpers();

        std::cout << "[RUN] test_roundtrip_small\n";
        test_roundtrip_small();

        std::cout << "[RUN] test_roundtrip_large_multi_chunk\n";
        test_roundtrip_large_multi_chunk();

        std::cout << "[RUN] test_empty_file\n";
        test_empty_file();

        std::cout << "[RUN] test_wrong_key_fails\n";
        test_wrong_key_fails();

        std::cout << "[RUN] test_cyphkey_wrap_unwrap_file\n";
        test_cyphkey_wrap_unwrap_file();

        std::cout << "[RUN] test_create_cyphkey_from_bytes_roundtrip\n";
        test_create_cyphkey_from_bytes_roundtrip();

        std::cout << "[RUN] test_pubkey_text_codec_and_fingerprint\n";
        test_pubkey_text_codec_and_fingerprint();

        std::cout << "[RUN] test_derive_shared_key_v1_symmetric_and_length\n";
        test_derive_shared_key_v1_symmetric_and_length();

    } catch (const std::exception& e) {
        std::cerr << "[EXCEPTION] " << e.what() << "\n";
        return 2;
    }

    if (g_fail == 0) {
        std::cout << "[OK] all tests passed\n";
        return 0;
    }
    std::cout << "[FAIL] total failed: " << g_fail << "\n";
    return 1;
}
