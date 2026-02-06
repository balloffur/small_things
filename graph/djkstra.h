#include "graph.h"
#include <deque>
#include <vector>
#include <iostream>
#include <utility>

int distance(const Graph& G, int a, int b) {
    int n = G.size;
    const int INF = std::numeric_limits<int>::max() / 4;

    std::vector<int> dist(n, INF);
    std::vector<char> used(n, 0);

    dist[a] = 0;

    for (int it = 0; it < n; ++it) {
        int v = -1;
        for (int i = 0; i < n; ++i) {
            if (!used[i] && (v == -1 || dist[i] < dist[v])) v = i;
        }
        if (v == -1 || dist[v] == INF) break; 
        if (v == b) break;

        used[v] = 1;

        for (int to = 0; to < n; ++to) {
            int w = G.matrix[v][to];
            if (w != 0 && dist[v] + w < dist[to]) {
                dist[to] = dist[v] + w;
            }
        }
    }

    return dist[b] == INF ? -1 : dist[b];
}
