#ifndef SORTNET_FAST_H
#define SORTNET_FAST_H

#include <array>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <utility>

namespace sortnet {

struct op {
    std::uint8_t i;
    std::uint8_t j;
};


template<class T>
inline void cswap(T& a, T& b) {
    if (b < a) { auto tmp = a; a = b; b = tmp; }
}

template<std::size_t N>
struct net;

template<> struct net<2> {
    inline static constexpr std::array<op, 1> ops = {{{0,1}}};
};

template<> struct net<3> {
    inline static constexpr std::array<op, 3> ops = {{{0,1},{1,2},{0,1}}};
};


template<> struct net<4> {
    inline static constexpr std::array<op, 5> ops = {{{0,1},{2,3},{0,2},{1,3},{1,2}}};
};


template<> struct net<5> {
    inline static constexpr std::array<op, 9> ops = {{
        {0,1},{3,4},{2,4},{2,3},{0,3},
        {1,4},{1,3},{0,2},{1,2}
    }};
};


template<> struct net<6> {
    inline static constexpr std::array<op, 12> ops = {{
        {1,2},{4,5},{0,2},{3,5},{0,1},{3,4},
        {2,5},{0,3},{1,4},{2,4},{1,3},{2,3}
    }};
};


template<> struct net<7> {
    inline static constexpr std::array<op, 16> ops = {{
        {0,6},{2,3},{4,5},
        {0,2},{1,4},{3,6},
        {0,1},{2,5},{3,4},
        {1,2},{4,6},
        {2,3},{4,5},
        {1,2},{3,4},{5,6}
    }};
};


template<> struct net<8> {
    inline static constexpr std::array<op, 19> ops = {{
        {0,1},{2,3},{4,5},{6,7},
        {0,2},{1,3},{4,6},{5,7},
        {1,2},{5,6},
        {0,4},{1,5},{2,6},{3,7},
        {2,4},{3,5},
        {1,2},{3,4},{5,6}
    }};
};


template<> struct net<9> {
    inline static constexpr std::array<op, 25> ops = {{
        {0,3},{1,7},{2,5},{4,8},
        {0,7},{2,4},{3,8},{5,6},
        {0,2},{1,3},{4,5},{7,8},
        {1,4},{3,6},{5,7},
        {0,1},{2,4},{3,5},{6,8},
        {2,3},{4,5},{6,7},
        {1,2},{3,4},{5,6}
    }};
};

template<> struct net<10> {
    inline static constexpr std::array<op, 29> ops = {{
        {0,8},{1,9},{2,7},{3,5},{4,6},
        {0,2},{1,4},{5,8},{7,9},
        {0,3},{2,4},{5,7},{6,9},
        {0,1},{3,6},{8,9},
        {1,5},{2,3},{4,8},{6,7},
        {1,2},{3,5},{4,6},{7,8},
        {2,3},{4,5},{6,7},
        {3,4},{5,6}
    }};
};


template<> struct net<11> {
    inline static constexpr std::array<op, 35> ops = {{
        {0,9},{1,6},{2,4},{3,7},{5,8},
        {0,1},{3,5},{4,10},{6,9},{7,8},
        {1,3},{2,5},{4,7},{8,10},
        {0,4},{1,2},{3,7},{5,9},{6,8},
        {0,1},{2,6},{4,5},{7,8},{9,10},
        {2,4},{3,6},{5,7},{8,9},
        {1,2},{3,4},{5,6},{7,8},
        {2,3},{4,5},{6,7}
    }};
};


template<> struct net<12> {
    inline static constexpr std::array<op, 39> ops = {{
        {0,8},{1,7},{2,6},{3,11},{4,10},{5,9},
        {0,1},{2,5},{3,4},{6,9},{7,8},{10,11},
        {0,2},{1,6},{5,10},{9,11},
        {0,3},{1,2},{4,6},{5,7},{8,11},{9,10},
        {1,4},{3,5},{6,8},{7,10},
        {1,3},{2,5},{6,9},{8,10},
        {2,3},{4,5},{6,7},{8,9},
        {4,6},{5,7},
        {3,4},{5,6},{7,8}
    }};
};


template<> struct net<13> {
    inline static constexpr std::array<op, 45> ops = {{
        {0,12},{1,10},{2,9},{3,7},{5,11},{6,8},
        {1,6},{2,3},{4,11},{7,9},{8,10},
        {0,4},{1,2},{3,6},{7,8},{9,10},{11,12},
        {4,6},{5,9},{8,11},{10,12},
        {0,5},{3,8},{4,7},{6,11},{9,10},
        {0,1},{2,5},{6,9},{7,8},{10,11},
        {1,3},{2,4},{5,6},{9,10},
        {1,2},{3,4},{5,7},{6,8},
        {2,3},{4,5},{6,7},{8,9},
        {3,4},{5,6}
    }};
};



template<> struct net<14> {
    inline static constexpr std::array<op, 51> ops = {{
        {0,1},{2,3},{4,5},{6,7},{8,9},{10,11},{12,13},
        {0,2},{1,3},{4,8},{5,9},{10,12},{11,13},
        {0,4},{1,2},{3,7},{5,8},{6,10},{9,13},{11,12},
        {0,6},{1,5},{3,9},{4,10},{7,13},{8,12},
        {2,10},{3,11},{4,6},{7,9},
        {1,3},{2,8},{5,11},{6,7},{10,12},
        {1,4},{2,6},{3,5},{7,11},{8,10},{9,12},
        {2,4},{3,6},{5,8},{7,10},{9,11},
        {3,4},{5,6},{7,8},{9,10},
        {6,7}
    }};
};


template<> struct net<15> {
    inline static constexpr std::array<op, 56> ops = {{
        {1,2},{3,10},{4,14},{5,8},{6,13},{7,12},{9,11},
        {0,14},{1,5},{2,8},{3,7},{6,9},{10,12},{11,13},
        {0,7},{1,6},{2,9},{4,10},{5,11},{8,13},{12,14},
        {0,6},{2,4},{3,5},{7,11},{8,10},{9,12},{13,14},
        {0,3},{1,2},{4,7},{5,9},{6,8},{10,11},{12,13},
        {0,1},{2,3},{4,6},{7,9},{10,12},{11,13},
        {1,2},{3,5},{8,10},{11,12},
        {3,4},{5,6},{7,8},{9,10},
        {2,3},{4,5},{6,7},{8,9},{10,11},
        {5,6},{7,8}
    }};
};


template<> struct net<16> {
    inline static constexpr std::array<op, 60> ops = {{
        {0,13},{1,12},{2,15},{3,14},{4,8},{5,6},{7,11},{9,10},
        {0,5},{1,7},{2,9},{3,4},{6,13},{8,14},{10,15},{11,12},
        {0,1},{2,3},{4,5},{6,8},{7,9},{10,11},{12,13},{14,15},
        {0,2},{1,3},{4,10},{5,11},{6,7},{8,9},{12,14},{13,15},
        {1,2},{3,12},{4,6},{5,7},{8,10},{9,11},{13,14},
        {1,4},{2,6},{5,8},{7,10},{9,13},{11,14},
        {2,4},{3,6},{9,12},{11,13},
        {3,5},{6,8},{7,9},{10,12},
        {3,4},{5,6},{7,8},{9,10},{11,12},
        {6,7},{8,9}
    }};
};


template<std::size_t N, class It, std::size_t... K>
inline void sort_n_impl(It begin, std::index_sequence<K...>) {
    // fold expression: cswap(...) для каждого компаратора
    (cswap(*(begin + net<N>::ops[K].i), *(begin + net<N>::ops[K].j)), ...);
}

template<std::size_t N, class It>
inline void sort_n(It begin) {
    sort_n_impl<N>(begin, std::make_index_sequence<net<N>::ops.size()>{});
}


// smallsort n<17
template<class It>
inline void smallsort(It begin, It end) {
    static_assert(std::is_same_v<
        typename std::iterator_traits<It>::iterator_category,
        std::random_access_iterator_tag> ||
        std::is_base_of_v<std::random_access_iterator_tag,
        typename std::iterator_traits<It>::iterator_category>,
        "sortnet::smallsort requires random access iterators");

    auto n = end - begin;
    if (n <= 1) return;

    switch ((int)n) {
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
        default:
            // Если больше 16, зовём квиксорт?
            std::sort(begin, end);
            break;
    }
}

} // namespace sortnet

#endif
