#pragma once
#include <cstdint>
#include <vector>
#include <utility>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <cstring>
#include <chrono>

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
    #include <concepts>
    #include <type_traits>
    #define LCG64_HAS_CONCEPTS 1
#else
    #define LCG64_HAS_CONCEPTS 0
#endif

/// Default seed value used when no seed is provided.
constexpr uint64_t DEFAULT_SEED = 0xDEADBEEFDEADBEEFULL;

/// Precomputed powers of ten used by uint64_digs().
static constexpr uint64_t pows_of_ten[] = {
    1ULL,10ULL,100ULL,1000ULL,10000ULL,100000ULL,1000000ULL,10000000ULL,
    100000000ULL,1000000000ULL,10000000000ULL,100000000000ULL,
    1000000000000ULL,10000000000000ULL,100000000000000ULL,
    1000000000000000ULL,10000000000000000ULL,100000000000000000ULL,
    1000000000000000000ULL,10000000000000000000ULL
};

/**
 * @brief Fast 64-bit PRNG based on a combined LCG + XorShift step.
 *
 * Compact (64-bit) internal state, deterministic for fixed seed.
 * Not cryptographically secure — intended for simulation, Monte-Carlo,
 * procedural generation, sampling, randomized algorithms etc.
 */
class PRNG64 {
    uint64_t state;
    static constexpr uint64_t A = 6364136223846793005ULL;
    static constexpr uint64_t C = 1ULL;
    static constexpr uint64_t DEFAULT_SEED = 0xDEADBEEFDEADBEEFULL;
    static const unsigned int MAX_COUNT_CONDITION_DEFAULT = 100000;
    static constexpr int XS_S1 = 12;
    static constexpr int XS_S2 = 25;
    static constexpr int XS_S3 = 27;

    public:
    

    
    /**
     * @brief Default deterministic seed constructor.
     */
    constexpr PRNG64() : state(DEFAULT_SEED) {}

    /**
     * @brief Construct PRNG from raw double bits.
     */
    explicit PRNG64(double seed){
        static_assert(sizeof(double) == 8, "unexpected double size");
        std::memcpy(&state, &seed, sizeof(seed));
    }

#if LCG64_HAS_CONCEPTS
    /**
     * @brief Construct from any type convertible to uint64_t.
     */
    template<typename T>
        requires (std::convertible_to<T, uint64_t> &&
                  !std::same_as<std::remove_cvref_t<T>, PRNG64>)
    explicit PRNG64(T seed) : state(static_cast<uint64_t>(seed)) {}
#else
    explicit constexpr PRNG64(uint64_t seed) : state(seed) {}
#endif

    /**
     * @brief Create a PRNG seeded from system time (non-deterministic).
     */
    static PRNG64 time_seed(){
        using namespace std::chrono;
        uint64_t t = high_resolution_clock::now().time_since_epoch().count();
        uint64_t stack = reinterpret_cast<uint64_t>(&t);
        uint64_t seed = t ^ (stack * 0x9E3779B97F4A7C15ULL);
        return PRNG64(seed);
    }

    /**
     * @brief Next random 64-bit value (LCG + XorShift).
     */
    uint64_t uint64(){
        state = state * A + C;
        uint64_t x = state;
        x ^= x >> XS_S1;
        x ^= x << XS_S2;
        x ^= x >> XS_S3;
        return x;
    }

private:
    // Внутренняя unbiased функция [0, n)
    uint64_t _next_exclusive(uint64_t n) {
        if (n <= 1) return 0;
        if ((n & (n-1)) == 0)                     // степень двойки — быстрый путь
            return uint64() & (n-1);

        uint64_t threshold = (-n) % n;            // Lemire’s unbiased method
        while (true) {
            uint64_t r = uint64();
            if (r >= threshold)
                return r % n;
        }
    }

public:
    /**
     * @brief Random integer in range [low, high].
     */
    uint64_t uint64(uint64_t low, uint64_t high){
        if (low > high) return low;               // защита от инвертированного диапазона
        uint64_t range = high - low + 1;
        if (range == 0) return low;               // переполнение при low=high=UINT64_MAX
        return low + _next_exclusive(range);
    }

    int64_t int64(int64_t low,int64_t high){
        if (low >= high) return low;
        uint64_t range = high - low + 1;
        return low+_next_exclusive(range);
    }

    /**
     * @brief Random integer in [0, high).
     */
    uint64_t uint64_exclusive(uint64_t high){
        return _next_exclusive(high);
    }

    /**
     * @brief Bernoulli(p) — fair 50/50 bit.
     */
    bool bit(){
        return uint64() & 1;
    }

    /**
     * @brief Bernoulli(p) — biased coin flip returning true ~p.
     */
    bool bit(double p){
        if(p <= 0.0) return false;
        if(p >= 1.0) return true;
        return real() < p;
    }

    /**
     * @brief Integer with exactly `digs` decimal digits.
     */
    uint64_t uint64_digs(int digs){
        if(digs <= 0 || digs > 19) return 0;
        uint64_t low   = pows_of_ten[digs - 1];
        uint64_t range = pows_of_ten[digs] - low;   // 9×10^{digs-1}
        return low + _next_exclusive(range);
    }

    

    /**
     * @brief Draw values until predicate passes.
     */
    template<typename Func>
    uint64_t uint64_cond(Func condition, unsigned int max_count = MAX_COUNT_CONDITION_DEFAULT){
        uint64_t r;
        if(max_count == 0){
            do {
                r = uint64();
            } while(!condition(r));
            return r;
        } else {
            for(unsigned int i = 0; i < max_count; ++i){
                r = uint64();
                if(condition(r)) return r;
            }
            return 0;        // исчерпано количество попыток
        }
    }

    /**
     * @brief Uniform double in [0,1).
     */
    double real(){
        // 100% кросс-платформенный и точный способ
        return (uint64() >> 11) * 0x1.0p-53;
    }

    /**
     * @brief Uniform double in [low, high).
     */
    double real(double low, double high){
        return low + real() * (high - low);
    }
};


/// @brief i->j edge is [i][j]
struct Graph{
    std::vector<std::vector<int>> matrix;
    int size;

    Graph():size(0),matrix(){}

    //Никаких проверок
    Graph(int n):size(n),matrix(std::vector<std::vector<int>>(n,std::vector<int>(n))){};
    //Никаких проверок
    Graph(const std::vector<std::vector<int>>& v):size(v.size()),matrix(v){
        if(v.size()>0 && v.size()!=v[0].size()){
            throw std::invalid_argument("Not quadratic matrix init");
        } 
    }



    void add_edge(int i,int j,int value=1){
        if(i<0 || j<0 || i>=size || j>=size){throw std::invalid_argument("Index out of bounds");}

        matrix[i][j]=value;
        matrix[j][i]=value;
    };

    void remove_edge(int i,int j){
        if(i<0 || j<0 || i>=size || j>=size){throw std::invalid_argument("Index out of bounds");}
        matrix[i][j]=0;
        matrix[j][i]=0;
    };

    void add_vert(){
        for(int i=0;i<size;i++){
            matrix[i].push_back(0);
        }
        matrix.push_back(std::vector<int>(size+1));
        size++;
    };

    void swap_vert(int i,int j){
        if(i<0 || j<0 || i>=size || j>=size){throw std::invalid_argument("Index out of bounds");}
        if(i==j){return;}
        for(int k=0;k<size;k++){
            std::swap(matrix[k][i],matrix[k][j]);
        }
        for(int k=0;k<size;k++){
            std::swap(matrix[i][k],matrix[j][k]);
        }

    };
    


    //removes last vert
    void pop_vert(){
        if(size<1){return;}
        for(int i=0;i<size-1;i++){
            matrix[i].pop_back();
        }
        matrix.pop_back();
        size--;
    };

    void remove_vert(int i){
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        if(i!=size-1) swap_vert(i,size-1);
        pop_vert();
    };

    int degree(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count++;
        }
        return count;
    }

    int degree_in(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count++;
        }
        return count;
    }

    int degree_out(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[k][i]!=0) count++;
        }
        return count;
    }

    int degree_oriented(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count++;
        }
        for(int k=0;k<size;k++){
            if(matrix[k][i]!=0) count++;
        }
        return count;
    }

     int weight(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count+=matrix[i][k];
        }
        return count;
    }

    int weight_in(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count+=matrix[i][k];
        }
        return count;
    }

    int weight_out(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count+=matrix[i][k];
        }
        return count;
    }

    int weight_oriented(int i) const {
        if(i<0 || i>=size){
            throw std::invalid_argument("Index out of bounds");
        }
        int count=0;
        for(int k=0;k<size;k++){
            if(matrix[i][k]!=0) count+=matrix[i][k];
        }
        return count;
    }


    void print() const {
        for(int i=0;i<size;i++){
            for(int j=0;j<size;j++){
                std::cout<<matrix[i][j]<<' ';
            }
            std::cout<<"\n";
        }
    }
};





/**
 * @file rgraph.h
 * @brief Random graph and tree generators.
 *
 * This header provides:
 *  - Undirected graphs: unweighted / weighted
 *  - Directed graphs: unweighted / weighted
 *  - Connected versions (undirected connected, directed weakly connected)
 *  - Strongly connected directed versions
 *  - Uniform random labeled trees via Prüfer sequence (unweighted / weighted)
 *
 * Notes:
 *  - Graph is represented by an adjacency matrix `matrix[i][j]`.
 *  - For unweighted graphs, value 1 means an edge exists, 0 means no edge.
 *  - For weighted graphs, 0 means no edge, positive value means edge weight.
 *  - Directed graphs use matrix[i][j] as edge i -> j.
 *  - Connected variants build a spanning structure first and then add random edges/arcs.
 */

/* ---------------------------- base generators ---------------------------- */

/**
 * @brief Generate an undirected unweighted random graph G(n, p).
 *
 * For each pair (i, j) with i < j, an edge is added with probability p and mirrored to keep symmetry.
 *
 * @param n Number of vertices.
 * @param p Edge probability in [0, 1].
 * @param seed Seed for PRNG.
 * @return Undirected unweighted graph with adjacency matrix.
 */
inline Graph random_graph(int n, double p, double seed) {
    PRNG64 RNG(seed);
    Graph ans(n);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (RNG.real() < p) {
                ans.matrix[i][j] = 1;
                ans.matrix[j][i] = 1;
            }
        }
    }
    return ans;
}

/**
 * @brief Generate an undirected weighted random graph G(n, p) with integer weights in [low, high].
 *
 * For each pair (i, j) with i < j, an edge is added with probability p. The weight is drawn uniformly
 * from [low, high] and mirrored to keep symmetry.
 *
 * @param n Number of vertices.
 * @param p Edge probability in [0, 1].
 * @param seed Seed for PRNG.
 * @param low Minimum edge weight (inclusive).
 * @param high Maximum edge weight (inclusive).
 * @return Undirected weighted graph with adjacency matrix.
 */
inline Graph random_graph_w(int n, double p, double seed, int low, int high) {
    PRNG64 RNG(seed);
    Graph ans(n);

    const uint64_t lo = (uint64_t)low;
    const uint64_t hi = (uint64_t)high;
    const uint64_t span = (hi >= lo) ? (hi - lo) : 0;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (RNG.real() < p) {
                int w = low;
                if (span > 0) w = (int)(lo + RNG.uint64(0, span));
                ans.matrix[i][j] = w;
                ans.matrix[j][i] = w;
            }
        }
    }
    return ans;
}

/**
 * @brief Generate a directed unweighted random graph on n vertices with edge probability p.
 *
 * For each ordered pair (i, j) with i != j, add arc i -> j with probability p.
 *
 * @param n Number of vertices.
 * @param p Arc probability in [0, 1].
 * @param seed Seed for PRNG.
 * @return Directed unweighted graph with adjacency matrix.
 */
inline Graph random_graph_o(int n, double p, double seed) {
    PRNG64 RNG(seed);
    Graph ans(n);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j && RNG.real() < p) {
                ans.matrix[i][j] = 1;
            }
        }
    }
    return ans;
}

/**
 * @brief Generate a directed weighted random graph on n vertices with arc probability p and weights in [low, high].
 *
 * For each ordered pair (i, j) with i != j, add arc i -> j with probability p.
 * The weight is drawn uniformly from [low, high].
 *
 * @param n Number of vertices.
 * @param p Arc probability in [0, 1].
 * @param seed Seed for PRNG.
 * @param low Minimum arc weight (inclusive).
 * @param high Maximum arc weight (inclusive).
 * @return Directed weighted graph with adjacency matrix.
 */
inline Graph random_graph_o_w(int n, double p, double seed, int low, int high) {
    PRNG64 RNG(seed);
    Graph ans(n);

    const uint64_t lo = (uint64_t)low;
    const uint64_t hi = (uint64_t)high;
    const uint64_t span = (hi >= lo) ? (hi - lo) : 0;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j && RNG.real() < p) {
                int w = low;
                if (span > 0) w = (int)(lo + RNG.uint64(0, span));
                ans.matrix[i][j] = w;
            }
        }
    }
    return ans;
}

/* ------------------------------ random trees ----------------------------- */

/**
 * @brief Generate a uniformly random labeled tree on n vertices using a random Prüfer sequence.
 *
 * The distribution is uniform over all labeled trees on {0, 1, ..., n-1}.
 *
 * @param n Number of vertices.
 * @param RNG External PRNG (state is advanced).
 * @return Undirected unweighted tree with adjacency matrix.
 */
inline Graph random_tree(int n, PRNG64& RNG) {
    Graph G(n);
    if (n < 2) return G;
    if (n == 2) {
        G.matrix[0][1] = 1;
        G.matrix[1][0] = 1;
        return G;
    }

    std::vector<int> p(n - 2), deg(n, 1);
    for (int i = 0; i < n - 2; i++) deg[p[i] = (int)RNG.int64(0, n - 1)]++;

    int ptr = 0;
    while (ptr < n && deg[ptr] != 1) ptr++;

    for (int x : p) {
        int u = ptr;
        G.matrix[u][x] = 1;
        G.matrix[x][u] = 1;
        deg[u]--;
        if (--deg[x] == 1 && x < ptr) ptr = x;
        else while (ptr < n && deg[ptr] != 1) ptr++;
    }

    int u = -1, v = -1;
    for (int i = 0; i < n; i++) {
        if (deg[i] == 1) {
            if (u == -1) u = i;
            else v = i;
        }
    }

    G.matrix[u][v] = 1;
    G.matrix[v][u] = 1;
    return G;
}

/**
 * @brief Generate a uniformly random labeled weighted tree on n vertices using a random Prüfer sequence.
 *
 * The distribution of the underlying tree is uniform over all labeled trees on {0, 1, ..., n-1}.
 * Each chosen edge gets an independent integer weight in [value_low, value_high].
 *
 * @param n Number of vertices.
 * @param RNG External PRNG (state is advanced).
 * @param value_low Minimum edge weight (inclusive).
 * @param value_high Maximum edge weight (inclusive).
 * @return Undirected weighted tree with adjacency matrix.
 */
inline Graph random_tree_w(int n, PRNG64& RNG, int value_low, int value_high) {
    Graph G(n);
    if (n < 2) return G;
    if (value_high < value_low) std::swap(value_low, value_high);

    auto draw_w = [&]() -> int {
        uint64_t span = (uint64_t)value_high - (uint64_t)value_low ;
        return value_low + (int)RNG.uint64(0, span);
    };

    if (n == 2) {
        int w = draw_w();
        G.matrix[0][1] = w;
        G.matrix[1][0] = w;
        return G;
    }

    std::vector<int> p(n - 2), deg(n, 1);
    for (int i = 0; i < n - 2; i++) deg[p[i] = (int)RNG.int64(0, n - 1)]++;

    int ptr = 0;
    while (ptr < n && deg[ptr] != 1) ptr++;

    for (int x : p) {
        int u = ptr;
        int w = draw_w();
        G.matrix[u][x] = w;
        G.matrix[x][u] = w;
        deg[u]--;
        if (--deg[x] == 1 && x < ptr) ptr = x;
        else while (ptr < n && deg[ptr] != 1) ptr++;
    }

    int u = -1, v = -1;
    for (int i = 0; i < n; i++) {
        if (deg[i] == 1) {
            if (u == -1) u = i;
            else v = i;
        }
    }

    int w = draw_w();
    G.matrix[u][v] = w;
    G.matrix[v][u] = w;
    return G;
}

/* -------------------------- connected graph helpers ---------------------- */

/**
 * @brief Generate an undirected connected unweighted random graph.
 *
 * First generates a random spanning tree (uniform labeled tree), then adds missing edges (i<j)
 * independently with probability p.
 *
 * @param n Number of vertices.
 * @param p Additional edge probability in [0, 1] for edges not in the spanning tree.
 * @param seed Seed for PRNG.
 * @return Connected undirected unweighted graph with adjacency matrix.
 */
inline Graph random_graph_c(int n, double p, double seed) {
    PRNG64 RNG(seed);
    Graph ans = random_tree(n, RNG);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (ans.matrix[i][j] != 0) continue;
            if (RNG.real() < p) {
                ans.matrix[i][j] = 1;
                ans.matrix[j][i] = 1;
            }
        }
    }
    return ans;
}

/**
 * @brief Generate an undirected connected weighted random graph with weights in [low, high].
 *
 * First generates a random spanning tree (uniform labeled tree) with weights in [low, high],
 * then adds missing edges (i<j) independently with probability p, assigning a random weight in [low, high].
 *
 * @param n Number of vertices.
 * @param p Additional edge probability in [0, 1] for edges not in the spanning tree.
 * @param seed Seed for PRNG.
 * @param low Minimum edge weight (inclusive).
 * @param high Maximum edge weight (inclusive).
 * @return Connected undirected weighted graph with adjacency matrix.
 */
inline Graph random_graph_w_c(int n, double p, double seed, int low, int high) {
    PRNG64 RNG(seed);
    Graph ans = random_tree_w(n, RNG, low, high);

    const uint64_t lo = (uint64_t)low;
    const uint64_t hi = (uint64_t)high;
    const uint64_t span = (hi >= lo) ? (hi - lo) : 0;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (ans.matrix[i][j] != 0) continue;
            if (RNG.real() < p) {
                int w = low;
                if (span > 0) w = (int)(lo + RNG.uint64(0, span));
                ans.matrix[i][j] = w;
                ans.matrix[j][i] = w;
            }
        }
    }
    return ans;
}

/**
 * @brief Generate a directed weakly connected unweighted random graph.
 *
 * Guarantees weak connectivity by first generating an undirected spanning tree and then orienting
 * each tree edge randomly. After that, adds missing arcs independently with probability p.
 *
 * @param n Number of vertices.
 * @param p Additional arc probability in [0, 1] for arcs not already present.
 * @param seed Seed for PRNG.
 * @return Directed unweighted graph that is weakly connected.
 */
inline Graph random_graph_o_c(int n, double p, double seed) {
    PRNG64 RNG(seed);
    Graph ans(n);

    Graph base = random_tree(n, RNG);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (base.matrix[i][j] != 0) {
                if (RNG.real() < 0.5) ans.matrix[i][j] = 1;
                else ans.matrix[j][i] = 1;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            if (ans.matrix[i][j] != 0) continue;
            if (RNG.real() < p) ans.matrix[i][j] = 1;
        }
    }

    return ans;
}

/**
 * @brief Generate a directed weakly connected weighted random graph with weights in [low, high].
 *
 * Guarantees weak connectivity by first generating an undirected spanning tree (unweighted) and then orienting
 * each tree edge randomly while assigning a random weight in [low, high]. After that, adds missing arcs
 * independently with probability p, assigning weights in [low, high].
 *
 * @param n Number of vertices.
 * @param p Additional arc probability in [0, 1] for arcs not already present.
 * @param seed Seed for PRNG.
 * @param low Minimum arc weight (inclusive).
 * @param high Maximum arc weight (inclusive).
 * @return Directed weighted graph that is weakly connected.
 */
inline Graph random_graph_o_w_c(int n, double p, double seed, int low, int high) {
    PRNG64 RNG(seed);
    Graph ans(n);

    Graph base = random_tree(n, RNG);

    const uint64_t lo = (uint64_t)low;
    const uint64_t hi = (uint64_t)high;
    const uint64_t span = (hi >= lo) ? (hi - lo) : 0;

    auto draw_w = [&]() -> int {
        int w = low;
        if (span > 0) w = (int)(lo + RNG.uint64(0, span));
        return w;
    };

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (base.matrix[i][j] != 0) {
                int w = draw_w();
                if (RNG.real() < 0.5) ans.matrix[i][j] = w;
                else ans.matrix[j][i] = w;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            if (ans.matrix[i][j] != 0) continue;
            if (RNG.real() < p) ans.matrix[i][j] = draw_w();
        }
    }

    return ans;
}

/* ----------------------- strongly connected (directed) ------------------- */

/**
 * @brief Generate a directed strongly connected unweighted random graph.
 *
 * Guarantees strong connectivity by first adding a directed Hamiltonian cycle:
 * 0 -> 1 -> ... -> n-1 -> 0. Then adds any missing arcs independently with probability p.
 *
 * @param n Number of vertices.
 * @param p Additional arc probability in [0, 1] for arcs not already present.
 * @param seed Seed for PRNG.
 * @return Directed unweighted graph that is strongly connected.
 */
inline Graph random_graph_o_sc(int n, double p, double seed) {
    PRNG64 RNG(seed);
    Graph ans(n);
    if (n <= 1) return ans;

    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        ans.matrix[i][j] = 1;
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            if (ans.matrix[i][j] != 0) continue;
            if (RNG.real() < p) ans.matrix[i][j] = 1;
        }
    }

    return ans;
}

/**
 * @brief Generate a directed strongly connected weighted random graph with weights in [low, high].
 *
 * Guarantees strong connectivity by first adding a directed Hamiltonian cycle:
 * 0 -> 1 -> ... -> n-1 -> 0, assigning each cycle arc a random weight in [low, high].
 * Then adds any missing arcs independently with probability p, assigning weights in [low, high].
 *
 * @param n Number of vertices.
 * @param p Additional arc probability in [0, 1] for arcs not already present.
 * @param seed Seed for PRNG.
 * @param low Minimum arc weight (inclusive).
 * @param high Maximum arc weight (inclusive).
 * @return Directed weighted graph that is strongly connected.
 */
inline Graph random_graph_o_w_sc(int n, double p, double seed, int low, int high) {
    PRNG64 RNG(seed);
    Graph ans(n);
    if (n <= 1) return ans;

    const uint64_t lo = (uint64_t)low;
    const uint64_t hi = (uint64_t)high;
    const uint64_t span = (hi >= lo) ? (hi - lo) : 0;

    auto draw_w = [&]() -> int {
        int w = low;
        if (span > 0) w = (int)(lo + RNG.uint64(0, span));
        return w;
    };

    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        ans.matrix[i][j] = draw_w();
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            if (ans.matrix[i][j] != 0) continue;
            if (RNG.real() < p) ans.matrix[i][j] = draw_w();
        }
    }

    return ans;
}
