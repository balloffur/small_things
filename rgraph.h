#pragma once

#include <cstdint>
#include <vector>
#include <utility>

#include "graph.h
#include "random_LCG.h"

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
