#include "graph.h"
#include <deque>
#include <vector>
#include <iostream>

bool is_connected(const Graph& G){
    int n = G.size;
    if(n==0){return true;}
    std::vector<int> v(n,0);
    std::deque<int> deq;
    v[0]=1;
    deq.push_back(0);
    while(!deq.empty()){
        int ver=deq.front();
        deq.pop_front();
        for(int i=0;i<n;i++){
            if(G.matrix[ver][i]!=0 && v[i]==0){
                deq.push_back(i);
                v[i]=1;
            }
        }
    }
    for(int i=0;i<n;i++){
        if(v[i]==0){return false;}
    }
    return true;
}

std::vector<std::vector<int>> components(const Graph& G){
    int n = G.size;
    std::vector<std::vector<int>> ans;
    if(n==0){return ans;}

    std::vector<int> v(n,0);
    std::deque<int> deq;

    int cur_component = 1;

    v[0] = cur_component;    
    deq.push_back(0);

    bool whole_graph = true;
    std::vector<int> cur = {0};

    while(whole_graph){

        while(!deq.empty()){
            int ver = deq.front();
            deq.pop_front();

            for(int i=0;i<n;i++){
                if(G.matrix[ver][i]!=0 && v[i]==0){
                    deq.push_back(i);
                    v[i] = cur_component;
                    cur.push_back(i);
                }
            }
        }

        ans.push_back(cur);
        cur_component++;

        whole_graph = false;
        for(int i=0;i<n;i++){
            if(v[i]==0){
                cur.clear();
                cur = {i};

                v[i] = cur_component;   
                deq.push_back(i);

                whole_graph = true;
                break;
            }
        }
    }
    return ans;
}




void print_components(const std::vector<std::vector<int>>& comps) {
    std::cout << "Components: " << comps.size() << "\n";

    for (size_t i = 0; i < comps.size(); ++i) {
        std::cout << "  [" << i << "] (" << comps[i].size() << "): ";

        for (int v : comps[i]) {
            std::cout << v << ' ';
        }
        std::cout << '\n';
    }
}
