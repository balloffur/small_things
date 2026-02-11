#pragma once
#include <cstdint>
#include <vector>
#include <utility>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <chrono>
#include "graph.h"
#include "rng64.h"

using namespace rng64;

/**
 * @file rgraph.h
 * @brief Генераторы случайных графов и деревьев.
 *
 * Реализованы:
 *  - ориентированные и неориентированные графы;
 *  - взвешенные и невзвешенные версии;
 *  - связные и сильно связные варианты;
 *  - равномерно случайные помеченные деревья (через последовательность Прюфера).
 *
 * Граф хранится в виде матрицы смежности matrix[i][j].
 * Значение 0 означает отсутствие ребра, положительное значение — наличие ребра
 * (или его вес в случае взвешенного графа).
 * Для ориентированных графов matrix[i][j] соответствует дуге i → j.
 */


/**
 * @brief Неориентированный невзвешенный случайный граф G(n, p).
 *
 * Для каждой пары вершин i < j ребро добавляется с вероятностью p.
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
 * @brief Неориентированный взвешенный случайный граф G(n, p).
 *
 * Вес каждого ребра — целое число из диапазона [low, high].
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
 * @brief Ориентированный невзвешенный случайный граф.
 *
 * Для каждой упорядоченной пары i ≠ j дуга i → j добавляется с вероятностью p.
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
 * @brief Ориентированный взвешенный случайный граф.
 *
 * Вес каждой дуги выбирается из диапазона [low, high].
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


/**
 * @brief Равномерно случайное помеченное дерево.
 *
 * Строится с использованием случайной последовательности Прюфера.
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
    for (int i = 0; i < n - 2; i++)
        deg[p[i] = RNG.integer(0, n - 1)]++;

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
 * @brief Взвешенное случайное помеченное дерево.
 *
 * Топология дерева равномерна, веса рёбер независимы
 * и лежат в диапазоне [value_low, value_high].
 */
inline Graph random_tree_w(int n, PRNG64& RNG, int value_low, int value_high) {
    Graph G(n);
    if (n < 2) return G;
    if (value_high < value_low) std::swap(value_low, value_high);

    auto draw_w = [&]() {
        uint64_t span = (uint64_t)value_high - (uint64_t)value_low;
        return value_low + (int)RNG.uint64(0, span);
    };

    if (n == 2) {
        int w = draw_w();
        G.matrix[0][1] = w;
        G.matrix[1][0] = w;
        return G;
    }

    std::vector<int> p(n - 2), deg(n, 1);
    for (int i = 0; i < n - 2; i++)
        deg[p[i] = RNG.integer(0, n - 1)]++;

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


/**
 * @brief Неориентированный связный невзвешенный граф.
 *
 * Сначала строится случайное остовное дерево,
 * затем недостающие рёбра добавляются с вероятностью p.
 */
inline Graph random_graph_c(int n, double p, double seed) {
    PRNG64 RNG(seed);
    Graph ans = random_tree(n, RNG);

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (ans.matrix[i][j] == 0 && RNG.real() < p) {
                ans.matrix[i][j] = 1;
                ans.matrix[j][i] = 1;
            }
        }
    }
    return ans;
}


/**
 * @brief Неориентированный связный взвешенный граф.
 *
 * Используется взвешенное случайное остовное дерево,
 * после чего добавляются дополнительные рёбра.
 */
inline Graph random_graph_w_c(int n, double p, double seed, int low, int high) {
    PRNG64 RNG(seed);
    Graph ans = random_tree_w(n, RNG, low, high);

    const uint64_t lo = (uint64_t)low;
    const uint64_t hi = (uint64_t)high;
    const uint64_t span = (hi >= lo) ? (hi - lo) : 0;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (ans.matrix[i][j] == 0 && RNG.real() < p) {
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
 * @brief Ориентированный слабо связный невзвешенный граф.
 *
 * Гарантия связности достигается ориентацией рёбер
 * случайного остовного дерева.
 */
inline Graph random_graph_o_c(int n, double p, double seed) {
    PRNG64 RNG(seed);
    Graph ans(n);

    Graph base = random_tree(n, RNG);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (base.matrix[i][j]) {
                if (RNG.real() < 0.5) ans.matrix[i][j] = 1;
                else ans.matrix[j][i] = 1;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j && ans.matrix[i][j] == 0 && RNG.real() < p)
                ans.matrix[i][j] = 1;
        }
    }
    return ans;
}


/**
 * @brief Ориентированный сильно связный невзвешенный граф.
 *
 * В качестве базовой структуры используется гамильтонов цикл.
 */
inline Graph random_graph_o_sc(int n, double p, double seed) {
    PRNG64 RNG(seed);
    Graph ans(n);
    if (n <= 1) return ans;

    for (int i = 0; i < n; i++)
        ans.matrix[i][(i + 1) % n] = 1;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j && ans.matrix[i][j] == 0 && RNG.real() < p)
                ans.matrix[i][j] = 1;
        }
    }
    return ans;
}
