#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <type_traits>

namespace sortnet {

struct op {
    std::uint8_t i;
    std::uint8_t j;
};

template<class T>
inline void cswap(T& a, T& b) {
    if (b < a) std::swap(a, b);
}

template<std::size_t N>
struct net;

/* ===== 2 ===== */
template<>
struct net<2> {
    static constexpr std::array<op, 1> ops = {{
        {0,1}
    }};
};

/* ===== 3 ===== */
template<>
struct net<3> {
    static constexpr std::array<op, 3> ops = {{
        {0,1},{1,2},{0,1}
    }};
};

/* ===== 4 ===== */
template<>
struct net<4> {
    static constexpr std::array<op, 5> ops = {{
        {0,1},{2,3},{0,2},{1,3},{1,2}
    }};
};

/* ===== 5 ===== */
template<>
struct net<5> {
    static constexpr std::array<op, 9> ops = {{
        {0,1},{3,4},{2,4},{2,3},{0,3},
        {1,4},{1,3},{0,2},{1,2}
    }};
};

/* ===== 6 ===== */
template<>
struct net<6> {
    static constexpr std::array<op, 12> ops = {{
        {1,2},{4,5},{0,2},{3,5},{0,1},{3,4},
        {2,5},{0,3},{1,4},{2,4},{1,3},{2,3}
    }};
};

/* ===== 7 ===== */
template<>
struct net<7> {
    static constexpr std::array<op, 16> ops = {{
        {0,6},{2,3},{4,5},{0,2},{1,4},{3,6},
        {0,1},{2,5},{3,4},{1,2},{4,6},
        {1,3},{2,4},{5,6},{2,3},{3,4}
    }};
};

/* ===== 8 ===== */
template<>
struct net<8> {
    static constexpr std::array<op, 19> ops = {{
        {0,1},{2,3},{4,5},{6,7},
        {0,2},{1,3},{4,6},{5,7},
        {1,2},{5,6},
        {0,4},{1,5},{2,6},{3,7},
        {2,4},{3,5},
        {1,2},{3,4},{5,6}
    }};
};

/* ===== 9 ===== */
template<>
struct net<9> {
    static constexpr std::array<op, 25> ops = {{
        {0,1},{3,4},{6,7},
        {1,2},{4,5},{7,8},
        {0,1},{3,4},{6,7},
        {0,3},{1,4},{2,5},{6,8},
        {1,3},{2,4},{5,7},
        {2,3},{4,6},{5,6},
        {3,4}
    }};
};

/* ===== 10 ===== */
template<>
struct net<10> {
    static constexpr std::array<op, 29> ops = {{
        {0,1},{2,3},{5,6},{7,8},
        {1,3},{5,7},
        {0,2},{6,8},{4,9},
        {0,5},{1,6},{2,7},{3,8},
        {1,5},{2,6},{3,7},{4,8},
        {2,5},{3,6},{4,7},
        {3,5},{4,6},
        {5,9}
    }};
};

/* ===== 11 ===== */
template<>
struct net<11> {
    static constexpr std::array<op, 35> ops = {{
        {0,1},{2,3},{4,5},{6,7},{8,9},
        {1,3},{5,7},
        {0,2},{4,6},{8,10},
        {1,5},{2,6},{3,7},{4,8},
        {3,5},{4,6},{7,9},
        {1,2},{3,4},{5,6},{7,8},{9,10}
    }};
};

/* ===== 12 ===== */
template<>
struct net<12> {
    static constexpr std::array<op, 39> ops = {{
        {0,1},{2,3},{4,5},{6,7},{8,9},{10,11},
        {1,3},{5,7},{9,11},
        {0,2},{4,6},{8,10},
        {1,5},{6,10},{5,9},
        {2,6},{3,7},{4,8},
        {3,5},{6,8},{2,4},{7,9},
        {1,2},{3,4},{5,6},{7,8}
    }};
};

/* ===== 13 ===== */
template<>
struct net<13> {
    static constexpr std::array<op, 45> ops = {{
        {0,1},{2,3},{4,5},{6,7},{8,9},{10,11},
        {1,3},{5,7},{9,11},
        {0,2},{4,6},{8,10},
        {1,5},{6,10},{5,9},
        {2,6},{3,7},{4,8},
        {11,12},
        {3,5},{6,8},{2,4},{7,9},{10,12},
        {1,2},{3,4},{5,6},{7,8},{9,10},{11,12}
    }};
};

/* ===== 14 ===== */
template<>
struct net<14> {
    static constexpr std::array<op, 51> ops = {{
        {0,1},{2,3},{4,5},{6,7},{8,9},{10,11},{12,13},
        {1,3},{5,7},{9,11},
        {0,2},{4,6},{8,10},{12,13},
        {1,5},{6,10},{9,13},{5,9},
        {2,6},{3,7},{4,8},{11,13},
        {3,5},{6,8},{2,4},{7,9},{10,12},
        {1,2},{3,4},{5,6},{7,8},{9,10},{11,12}
    }};
};

/* ===== 15 ===== */
template<>
struct net<15> {
    static constexpr std::array<op, 56> ops = {{
        {0,1},{2,3},{4,5},{6,7},{8,9},{10,11},{12,13},
        {1,3},{5,7},{9,11},{13,14},
        {0,2},{4,6},{8,10},{12,14},
        {1,5},{6,10},{9,13},{5,9},
        {2,6},{3,7},{4,8},{11,14},
        {3,5},{6,8},{2,4},{7,9},{10,12},{11,13},
        {1,2},{3,4},{5,6},{7,8},{9,10},{11,12},{13,14}
    }};
};

/* ===== 16 ===== */
template<>
struct net<16> {
    static constexpr std::array<op, 60> ops = {{
        {0,1},{2,3},{4,5},{6,7},{8,9},{10,11},{12,13},{14,15},
        {1,3},{5,7},{9,11},{13,15},
        {0,2},{4,6},{8,10},{12,14},
        {1,5},{6,10},{9,13},{5,9},
        {2,6},{3,7},{4,8},{11,15},
        {3,5},{6,8},{2,4},{7,9},{10,12},{11,13},
        {1,2},{3,4},{5,6},{7,8},{9,10},{11,12},{13,14}
    }};
};

// Sorting fixed size, takes iterator 
template<std::size_t N, class It>
inline void sort_n(It begin) {
    for (auto [i,j] : net<N>::ops)
        cswap(*(begin + i), *(begin + j));
}

template<class It>
inline void smallsort(It begin, It end) {
    using diff_t = typename std::iterator_traits<It>::difference_type;

    diff_t n = end - begin;
    if (n <= 1) return;

    if (n <= 16) {
        switch (static_cast<int>(n)) {
            case 2:  sort_n<2>(begin);  break;
            case 3:  sort_n<3>(begin);  break;
            case 4:  sort_n<4>(begin);  break;
            case 5:  sort_n<5>(begin);  break;
            case 6:  sort_n<6>(begin);  break;
            case 7:  sort_n<7>(begin);  break;
            case 8:  sort_n<8>(begin);  break;
            case 9:  sort_n<9>(begin);  break;
            case 10: sort_n<10>(begin); break;
            case 11: sort_n<11>(begin); break;
            case 12: sort_n<12>(begin); break;
            case 13: sort_n<13>(begin); break;
            case 14: sort_n<14>(begin); break;
            case 15: sort_n<15>(begin); break;
            case 16: sort_n<16>(begin); break;
            default: break;
        }
        return;
    }
}

} // namespace sortnet
