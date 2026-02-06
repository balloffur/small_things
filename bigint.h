#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <utility>
#include <complex>
#include <cstdint>

static const int32_t BASE_DIGITS = 9;
static const int32_t BASE = 1000000000;
static const double PI = 3.14159265358979323846;
static const int64_t powers_of_2[30] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216, 33554432, 67108864, 134217728, 268435456, 536870912};
//используются для выбора алгоритма умножения
static const int32_t SIMPLE_MULT_BORDER = 10000;
static const int32_t FFT_BORDER = 1000000;

struct bigint {
    int32_t sign;
    std::vector<int32_t> digits;

    bigint() : sign(1) {}

    bigint(int64_t v) { *this = v; }

    bigint& operator=(int64_t v) {
        sign = 1;
        if (v < 0) {
            sign = -1;
            v = -v;
        }
        digits.clear();
        for (; v > 0; v = v / BASE) digits.push_back(v % BASE);
        return *this;
    }

    bigint(const std::string& s) { read(s); }

    void read(const std::string& s) {
        sign = 1;
        digits.clear();
        int32_t pos = 0;
        while (pos < (int32_t)s.size() && (s[pos] == '-' || s[pos] == '+')) {
            if (s[pos] == '-') sign = -sign;
            ++pos;
        }
        for (int32_t i = (int32_t)s.size() - 1; i >= pos; i -= BASE_DIGITS) {
            int32_t x = 0;
            for (int32_t j = std::max(pos, i - BASE_DIGITS + 1); j <= i; j++) x = x * 10 + s[j] - '0';
            digits.push_back(x);
        }
        trim();
    }

    friend std::istream& operator>>(std::istream& stream, bigint& v) {
        std::string s;
        stream >> s;
        v.read(s);
        return stream;
    }

    friend std::ostream& operator<<(std::ostream& stream, const bigint& v) {
        if (v.sign == -1 && !v.isZero()) stream << '-';
        stream << (v.digits.empty() ? 0 : v.digits.back());
        for (int32_t i = (int32_t)v.digits.size() - 2; i >= 0; --i)
            stream << std::setw(BASE_DIGITS) << std::setfill('0') << v.digits[i];
        return stream;
    }

    bool operator<(const bigint& v) const {
        if (sign != v.sign) return sign < v.sign;
        if (digits.size() != v.digits.size()) return digits.size() * sign < v.digits.size() * v.sign;
        for (int32_t i = (int32_t)digits.size() - 1; i >= 0; i--)
            if (digits[i] != v.digits[i]) return digits[i] * sign < v.digits[i] * sign;
        return false;
    }

    bool operator>(const bigint& v) const { return v < *this; }
    bool operator<=(const bigint& v) const { return !(v < *this); }
    bool operator>=(const bigint& v) const { return !(*this < v); }
    bool operator==(const bigint& v) const { return !(*this < v) && !(v < *this); }
    bool operator!=(const bigint& v) const { return *this < v || v < *this; }

    friend int32_t __compare_abs(const bigint& x, const bigint& y) {
        if (x.digits.size() != y.digits.size()) return x.digits.size() < y.digits.size() ? -1 : 1;
        for (int32_t i = (int32_t)x.digits.size() - 1; i >= 0; --i) {
            if (x.digits[i] != y.digits[i]) return x.digits[i] < y.digits[i] ? -1 : 1;
        }
        return 0;
    }

    bigint operator-() const {
        bigint res = *this;
        if (isZero()) return res;
        res.sign = -sign;
        return res;
    }

    void __int32_ternal_add(const bigint& v) {
        if (digits.size() < v.digits.size()) digits.resize(v.digits.size(), 0);
        for (int32_t i = 0, carry = 0; i < (int32_t)std::max(digits.size(), v.digits.size()) || carry; ++i) {
            if (i == (int32_t)digits.size()) digits.push_back(0);
            digits[i] += carry + (i < (int32_t)v.digits.size() ? v.digits[i] : 0);
            carry = digits[i] >= BASE;
            if (carry) digits[i] -= BASE;
        }
    }

    void __int32_ternal_sub(const bigint& v) {
        for (int32_t i = 0, carry = 0; i < (int32_t)v.digits.size() || carry; ++i) {
            digits[i] -= carry + (i < (int32_t)v.digits.size() ? v.digits[i] : 0);
            carry = digits[i] < 0;
            if (carry) digits[i] += BASE;
        }
        trim();
    }

    bigint operator+=(const bigint& v) {
        if (sign == v.sign) {
            __int32_ternal_add(v);
        } else {
            if (__compare_abs(*this, v) >= 0) {
                __int32_ternal_sub(v);
            } else {
                bigint vv = v;
                std::swap(*this, vv);
                __int32_ternal_sub(vv);
            }
        }
        return *this;
    }

    bigint operator-=(const bigint& v) {
        if (sign == v.sign) {
            if (__compare_abs(*this, v) >= 0) {
                __int32_ternal_sub(v);
            } else {
                bigint vv = v;
                std::swap(*this, vv);
                __int32_ternal_sub(vv);
                sign = -sign;
            }
        } else {
            __int32_ternal_add(v);
        }
        return *this;
    }

    template <typename L, typename R>
    typename std::enable_if<
        std::is_convertible<L, bigint>::value &&
        std::is_convertible<R, bigint>::value &&
        std::is_lvalue_reference<R&&>::value,
        bigint>::type friend operator+(L&& l, R&& r) {
        bigint result(std::forward<L>(l));
        result += r;
        return result;
    }

    template <typename L, typename R>
    typename std::enable_if<
        std::is_convertible<L, bigint>::value &&
        std::is_convertible<R, bigint>::value &&
        std::is_rvalue_reference<R&&>::value,
        bigint>::type friend operator+(L&& l, R&& r) {
        bigint result(std::move(r));
        result += l;
        return result;
    }

    template <typename L, typename R>
    typename std::enable_if<
        std::is_convertible<L, bigint>::value &&
        std::is_convertible<R, bigint>::value,
        bigint>::type friend operator-(L&& l, R&& r) {
        bigint result(std::forward<L>(l));
        result -= r;
        return result;
    }

    friend std::pair<bigint, bigint> divmod(const bigint& a1, const bigint& b1) {
        int64_t norm = BASE / (b1.digits.back() + 1);
        bigint a = a1.abs() * norm;
        bigint b = b1.abs() * norm;
        bigint q = 0, r = 0;
        q.digits.resize(a.digits.size());

        for (int32_t i = (int32_t)a.digits.size() - 1; i >= 0; i--) {
            r *= BASE;
            r += a.digits[i];
            int64_t s1 = r.digits.size() <= b.digits.size() ? 0 : r.digits[b.digits.size()];
            int64_t s2 = r.digits.size() <= b.digits.size() - 1 ? 0 : r.digits[b.digits.size() - 1];
            int64_t d = ((int64_t)BASE * s1 + s2) / b.digits.back();
            r -= b * d;
            while (r < 0) {
                r += b, --d;
            }
            q.digits[i] = (int32_t)d;
        }

        q.sign = a1.sign * b1.sign;
        r.sign = a1.sign;
        q.trim();
        r.trim();
        auto res = std::make_pair(q, r / norm);
        if (res.second < 0) res.second += b1;
        return res;
    }

    bigint operator/(const bigint& v) const {
        if (v < 0) return divmod(-*this, -v).first;
        return divmod(*this, v).first;
    }

    bigint operator%(const bigint& v) const { return divmod(*this, v).second; }

    bigint& operator%=(const bigint& v) {
        *this = divmod(*this, v).second;
        return *this;
    }

    void operator/=(int32_t v) {
        if (llabs(v) >= BASE) {
            *this /= bigint(v);
            return;
        }
        if (v < 0) sign = -sign, v = -v;
        for (int32_t i = (int32_t)digits.size() - 1, rem = 0; i >= 0; --i) {
            int64_t cur = digits[i] + rem * (int64_t)BASE;
            digits[i] = (int32_t)(cur / v);
            rem = (int32_t)(cur % v);
        }
        trim();
    }

    bigint operator/(int32_t v) const {
        if (llabs(v) >= BASE) return *this / bigint(v);
        bigint res = *this;
        res /= v;
        return res;
    }

    void operator/=(const bigint& v) { *this = *this / v; }

    int32_t operator%(int32_t v) const {
        if (BASE % v == 0) return (digits.size() == 0 ? 0 : (digits[0] % v) * sign);
        int32_t m = 0;
        for (int32_t i = (int32_t)digits.size() - 1; i >= 0; --i)
            m = (digits[i] + m * (int64_t)BASE) % v;
        return m * sign;
    }

    int64_t operator%(int64_t v) const {
        int32_t m = 0;
        for (int32_t i = (int32_t)digits.size() - 1; i >= 0; --i)
            m = (digits[i] + m * (int64_t)BASE) % v;
        return (int64_t)m * sign;
    }

    void operator*=(int32_t v) {
        if (llabs(v) >= BASE) {
            *this *= bigint(v);
            return;
        }
        if (v < 0) sign = -sign, v = -v;
        for (int32_t i = 0, carry = 0; i < (int32_t)digits.size() || carry; ++i) {
            if (i == (int32_t)digits.size()) digits.push_back(0);
            int64_t cur = digits[i] * (int64_t)v + carry;
            carry = (int32_t)(cur / BASE);
            digits[i] = (int32_t)(cur % BASE);
        }
        trim();
    }

    bigint operator*(int32_t v) const {
        if (llabs(v) >= BASE) return *this * bigint(v);
        bigint res = *this;
        res *= v;
        return res;
    }

    static std::vector<int32_t> convert_base(const std::vector<int32_t>& a, int32_t old_digits, int32_t new_digits) {
        std::vector<int64_t> p(std::max(old_digits, new_digits) + 1);
        p[0] = 1;
        for (int32_t i = 1; i < (int32_t)p.size(); i++) p[i] = p[i - 1] * 10;
        std::vector<int32_t> res;
        int64_t cur = 0;
        int32_t cur_digits = 0;
        for (int32_t i = 0; i < (int32_t)a.size(); i++) {
            cur += a[i] * p[cur_digits];
            cur_digits += old_digits;
            while (cur_digits >= new_digits) {
                res.push_back((int32_t)(cur % p[new_digits]));
                cur /= p[new_digits];
                cur_digits -= new_digits;
            }
        }
        res.push_back((int32_t)cur);
        while (!res.empty() && !res.back()) res.pop_back();
        return res;
    }

    void fft(std::vector<std::complex<double>>& x, bool invert) const {
        int32_t n = (int32_t)x.size();

        for (int32_t i = 1, j = 0; i < n; ++i) {
            int32_t bit = n >> 1;
            for (; j >= bit; bit >>= 1) j -= bit;
            j += bit;
            if (i < j) std::swap(x[i], x[j]);
        }

        for (int32_t len = 2; len <= n; len <<= 1) {
            double ang = 2 * PI / len * (invert ? -1 : 1);
            std::complex<double> wlen(cos(ang), sin(ang));
            for (int32_t i = 0; i < n; i += len) {
                std::complex<double> w(1);
                for (int32_t j = 0; j < len / 2; ++j) {
                    std::complex<double> u = x[i + j];
                    std::complex<double> v = x[i + j + len / 2] * w;
                    x[i + j] = u + v;
                    x[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
        if (invert)
            for (int32_t i = 0; i < n; ++i) x[i] /= n;
    }

    void multiply_fft(const std::vector<int32_t>& x, const std::vector<int32_t>& y, std::vector<int32_t>& res) const {
        std::vector<std::complex<double>> fa(x.begin(), x.end());
        std::vector<std::complex<double>> fb(y.begin(), y.end());
        int32_t n = 1;
        while (n < (int32_t)std::max(x.size(), y.size())) n <<= 1;
        n <<= 1;
        fa.resize(n);
        fb.resize(n);

        fft(fa, false);
        fft(fb, false);
        for (int32_t i = 0; i < n; ++i) fa[i] *= fb[i];
        fft(fa, true);

        res.resize(n);
        int64_t carry = 0;
        for (int32_t i = 0; i < n; ++i) {
            int64_t t = (int64_t)(fa[i].real() + 0.5) + carry;
            carry = t / 1000;
            res[i] = (int32_t)(t % 1000);
        }
    }

    bigint mul_simple(const bigint& v) const {
        bigint res;
        res.sign = sign * v.sign;
        res.digits.resize(digits.size() + v.digits.size());
        for (int32_t i = 0; i < (int32_t)digits.size(); ++i)
            if (digits[i])
                for (int32_t j = 0, carry = 0; j < (int32_t)v.digits.size() || carry; ++j) {
                    int64_t cur = res.digits[i + j] + (int64_t)digits[i] * (j < (int32_t)v.digits.size() ? v.digits[j] : 0) + carry;
                    carry = (int32_t)(cur / BASE);
                    res.digits[i + j] = (int32_t)(cur % BASE);
                }
        res.trim();
        return res;
    }

    typedef std::vector<int64_t> vll;

    static vll karatsubaMultiply(const vll& a, const vll& b) {
        int32_t n = (int32_t)a.size();
        vll res(n + n);
        if (n <= 32) {
            for (int32_t i = 0; i < n; i++)
                for (int32_t j = 0; j < n; j++) res[i + j] += a[i] * b[j];
            return res;
        }

        int32_t k = n >> 1;
        vll a1(a.begin(), a.begin() + k);
        vll a2(a.begin() + k, a.end());
        vll b1(b.begin(), b.begin() + k);
        vll b2(b.begin() + k, b.end());

        vll a1b1 = karatsubaMultiply(a1, b1);
        vll a2b2 = karatsubaMultiply(a2, b2);

        for (int32_t i = 0; i < k; i++) a2[i] += a1[i];
        for (int32_t i = 0; i < k; i++) b2[i] += b1[i];

        vll r = karatsubaMultiply(a2, b2);
        for (int32_t i = 0; i < (int32_t)a1b1.size(); i++) r[i] -= a1b1[i];
        for (int32_t i = 0; i < (int32_t)a2b2.size(); i++) r[i] -= a2b2[i];

        for (int32_t i = 0; i < (int32_t)r.size(); i++) res[i + k] += r[i];
        for (int32_t i = 0; i < (int32_t)a1b1.size(); i++) res[i] += a1b1[i];
        for (int32_t i = 0; i < (int32_t)a2b2.size(); i++) res[i + n] += a2b2[i];
        return res;
    }

    bigint mul_karatsuba(const bigint& v) const {
        std::vector<int32_t> x6 = convert_base(digits, BASE_DIGITS, 6);
        std::vector<int32_t> y6 = convert_base(v.digits, BASE_DIGITS, 6);
        vll x(x6.begin(), x6.end());
        vll y(y6.begin(), y6.end());
        while (x.size() < y.size()) x.push_back(0);
        while (y.size() < x.size()) y.push_back(0);
        while (x.size() & (x.size() - 1)) x.push_back(0), y.push_back(0);
        vll c = karatsubaMultiply(x, y);
        bigint res;
        res.sign = sign * v.sign;
        int64_t carry = 0;
        for (int32_t i = 0; i < (int32_t)c.size(); i++) {
            int64_t cur = c[i] + carry;
            res.digits.push_back((int32_t)(cur % 1000000));
            carry = cur / 1000000;
        }
        res.digits = convert_base(res.digits, 6, BASE_DIGITS);
        res.trim();
        return res;
    }

    void operator*=(const bigint& v) { *this = *this * v; }

    bigint operator*(const bigint& v) const {
        if ((int64_t)digits.size() * (int64_t)v.digits.size() <= SIMPLE_MULT_BORDER) return mul_simple(v);
        if ((int32_t)digits.size() < FFT_BORDER || (int32_t)v.digits.size() < FFT_BORDER) return mul_fft(v);
        return mul_karatsuba(v);
    }

    bigint mul_fft(const bigint& v) const {
        bigint res;
        res.sign = sign * v.sign;
        multiply_fft(convert_base(digits, BASE_DIGITS, 3), convert_base(v.digits, BASE_DIGITS, 3), res.digits);
        res.digits = convert_base(res.digits, 3, BASE_DIGITS);
        res.trim();
        return res;
    }

    bigint abs() const {
        bigint res = *this;
        res.sign *= res.sign;
        return res;
    }

    void trim() {
        while (!digits.empty() && !digits.back()) digits.pop_back();
        if (digits.empty()) sign = 1;
    }

    bool isZero() const { return digits.empty() || (digits.size() == 1 && !digits[0]); }
    bool isNegative() const { return !isZero() && (sign == -1); }
    bool isPositive() const { return !isZero() && (sign == 1); }

    friend bigint sqrt(const bigint& a1) {
        bigint a = a1;
        while (a.digits.empty() || a.digits.size() % 2 == 1) a.digits.push_back(0);

        int32_t n = (int32_t)a.digits.size();

        int32_t firstDigit = (int32_t)sqrt((double)a.digits[n - 1] * BASE + a.digits[n - 2]);
        int32_t norm = BASE / (firstDigit + 1);
        a *= norm;
        a *= norm;
        while (a.digits.empty() || a.digits.size() % 2 == 1) a.digits.push_back(0);

        bigint r = (int64_t)a.digits[n - 1] * BASE + a.digits[n - 2];
        firstDigit = (int32_t)sqrt((double)a.digits[n - 1] * BASE + a.digits[n - 2]);
        int32_t q = firstDigit;
        bigint res;

        for (int32_t j = n / 2 - 1; j >= 0; j--) {
            for (;; --q) {
                bigint r1 = (r - (res * 2 * bigint(BASE) + q) * q) * bigint(BASE) * bigint(BASE) +
                            (j > 0 ? (int64_t)a.digits[2 * j - 1] * BASE + a.digits[2 * j - 2] : 0);
                if (r1 >= 0) {
                    r = r1;
                    break;
                }
            }
            res *= BASE;
            res += q;

            if (j > 0) {
                int32_t d1 = res.digits.size() + 2 < r.digits.size() ? r.digits[res.digits.size() + 2] : 0;
                int32_t d2 = res.digits.size() + 1 < r.digits.size() ? r.digits[res.digits.size() + 1] : 0;
                int32_t d3 = res.digits.size() < r.digits.size() ? r.digits[res.digits.size()] : 0;
                q = ((int64_t)d1 * BASE * BASE + (int64_t)d2 * BASE + d3) / (firstDigit * 2);
            }
        }

        res.trim();
        return res / norm;
    }

    bool odd() const { return !isZero() && (digits[0] % 2 == 1); }
    bool even() const { return isZero() || (digits[0] % 2 == 0); }

    int32_t number_of_digits() const {
        int32_t ans = ((int32_t)digits.size() - 1) * 9;
        if (digits.back() > 99999999) return ans + 9;
        if (digits.back() > 9999999) return ans + 8;
        if (digits.back() > 999999) return ans + 7;
        if (digits.back() > 99999) return ans + 6;
        if (digits.back() > 9999) return ans + 5;
        if (digits.back() > 999) return ans + 4;
        if (digits.back() > 99) return ans + 3;
        if (digits.back() > 9) return ans + 2;
        return ans + 1;
    }

    std::string to_string() const {
        if (isZero()) return "0";
        std::string ans;
        if (sign == -1) ans = "-";
        ans += std::to_string(digits.back());
        for (int32_t i = (int32_t)digits.size() - 2; i >= 0; --i) {
            std::string temp = std::to_string(digits[i]);
            ans.append(9 - (int32_t)temp.size(), '0');
            ans += temp;
        }
        return ans;
    }

    bigint& operator++() {
        if (isZero()) {
            *this = 1;
        } else if (sign == 1) {
            digits[0]++;
            for (int32_t i = 0; i < (int32_t)digits.size() - 1; i++) {
                if (digits[i] < BASE) return *this;
                ++digits[i + 1];
                digits[i] = 0;
            }
            if (digits.back() == BASE) {
                digits.back() = 0;
                digits.push_back(1);
            }
        } else {
            for (int32_t i = 0; i < (int32_t)digits.size(); i++) {
                if (digits[i] != 0) {
                    --digits[i];
                    for (int32_t j = 0; j < i; j++) digits[j] = BASE - 1;
                }
                return *this;
            }
        }
        return *this;
    }

    bigint operator++(int32_t) {
        bigint ans = *this;
        ++*this;
        return ans;
    }

    bigint& operator--() {
        if (isZero()) {
            *this = -1;
        } else if (sign == -1) {
            digits[0]++;
            for (int32_t i = 0; i < (int32_t)digits.size() - 1; i++) {
                if (digits[i] < BASE) return *this;
                ++digits[i + 1];
                digits[i] = 0;
            }
            if (digits.back() == BASE) {
                digits.back() = 0;
                digits.push_back(1);
            }
        } else {
            for (int32_t i = 0; i < (int32_t)digits.size(); i++) {
                if (digits[i] != 0) {
                    --digits[i];
                    for (int32_t j = 0; j < i; j++) digits[j] = BASE - 1;
                }
                return *this;
            }
        }
        return *this;
    }

    bigint operator--(int32_t) {
        bigint ans = *this;
        --*this;
        return ans;
    }

    bigint& operator>>=(int32_t n) {
        if (isZero()) return *this;
        while (n > 9) {
            digits[0] /= powers_of_2[9];
            for (int32_t i = 1; i < (int32_t)digits.size(); ++i) {
                digits[i - 1] += (BASE / powers_of_2[9]) * (digits[i] % powers_of_2[9]);
                digits[i] /= powers_of_2[9];
            }
            n -= 9;
        }
        digits[0] /= powers_of_2[n];
        for (int32_t i = 1; i < (int32_t)digits.size(); ++i) {
            digits[i - 1] += (BASE / powers_of_2[n]) * (digits[i] % powers_of_2[n]);
            digits[i] /= powers_of_2[n];
        }
        trim();
        return *this;
    }

    bigint operator>>(int32_t n) const {
        bigint ans = *this;
        ans >>= n;
        return ans;
    }

    bigint& operator<<=(int32_t n) {
        while (n > 29) {
            int64_t carry = 0;
            int64_t temp = 0;
            for (int32_t i = 0; i < (int32_t)digits.size(); i++) {
                temp = powers_of_2[29] * (int64_t)digits[i];
                digits[i] = (int32_t)(temp % BASE + carry);
                carry = temp / BASE;
            }
            if (carry) digits.push_back((int32_t)carry);
            n -= 29;
        }
        int64_t carry = 0;
        int64_t temp = 0;
        for (int32_t i = 0; i < (int32_t)digits.size(); i++) {
            temp = powers_of_2[n] * (int64_t)digits[i];
            digits[i] = (int32_t)(temp % BASE + carry);
            carry = temp / BASE;
        }
        if (carry) digits.push_back((int32_t)carry);
        return *this;
    }

    bigint operator<<(int32_t n) const {
        bigint ans = *this;
        ans <<= n;
        return ans;
    }

    bigint& pow_mod(bigint power, const bigint& mod) {
        bigint a = *this;
        *this = 1;
        while (power.isPositive()) {
            if (power.digits[0] & 1) {
                *this *= a;
                *this = *this % mod;
            }
            a *= a;
            a = a % mod;
            power >>= 1;
        }
        return *this;
    }

    int32_t possible_shifts_to_odd() {
        int32_t ans = 0;
        for (int32_t i = 0; i < (int32_t)digits.size(); i++) {
            if (digits[i] % powers_of_2[9] == 0) {
                ans += 9;
            } else {
                int32_t j = 1;
                while (digits[i] % powers_of_2[j] == 0) {
                    ++ans;
                    ++j;
                }
                break;
            }
        }
        return ans;
    }

    int32_t shift_to_odd() {
        int32_t ans = 0;
        while (even()) {
            int32_t temp = possible_shifts_to_odd();
            *this >>= temp;
            ans += temp;
        }
        return ans;
    }
};

bigint gcd(bigint a, bigint b) {
    if (a.isNegative()) a.sign = 1;
    if (b.isNegative()) b.sign = 1;
    if (a.isZero() && b.isZero()) return 0;
    if (a.isZero()) return b;
    if (b.isZero()) return a;
    int32_t pow2 = std::min(a.shift_to_odd(), b.shift_to_odd());
    while (a != 0 && b != 0) {
        b.shift_to_odd();
        a.shift_to_odd();
        if (a > b) a -= b;
        else b -= a;
    }
    bigint ans = (a == 0 ? b : a);
    ans <<= pow2;
    return ans;
}

bigint lcm(bigint a, bigint b) {
    bigint ans = a;
    b = b / gcd(a, b);
    ans *= b;
    return ans;
}

bigint pow_mod(bigint number, bigint power, const bigint& mod) {
    bigint ans = 1;
    number = number % mod;
    while (power.isPositive()) {
        if (power.digits[0] & 1) {
            ans *= number;
            ans = ans % mod;
        }
        number *= number;
        number = number % mod;
        power >>= 1;
    }
    return ans;
}

double to_double(bigint a) {
    if (a.isZero()) return 0.0;
    int32_t i = (int32_t)a.digits.size() - 1;
    double ans = 0.0;
    while (i >= 0) {
        ans *= 1000000000.0;
        ans += (double)a.digits[i];
        --i;
    }
    if (a.isNegative()) return -ans;
    return ans;
}
