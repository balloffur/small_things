#include "rgraph.h"

#include <iostream>
#include <vector>
#include <fstream>
#include <string>

Graph generate_graph(bool wgh,
                     bool oriented,
                     bool connected,
                     bool strongly_connected,
                     int value_low,
                     int value_high,
                     double prob_low,
                     double prob_high,
                     int n_low,
                     int n_high,
                     PRNG64& RNG)
{
    int n = RNG.int64(n_low, n_high);
    double p = RNG.real(prob_low, prob_high);
    double seed = RNG.real();

    if (oriented) {
        if (strongly_connected) {
            if (wgh) return random_graph_o_w_sc(n, p, seed, value_low, value_high);
            return random_graph_o_sc(n, p, seed);
        }
        if (connected) {
            if (wgh) return random_graph_o_w_c(n, p, seed, value_low, value_high);
            return random_graph_o_c(n, p, seed);
        }
        if (wgh) return random_graph_o_w(n, p, seed, value_low, value_high);
        return random_graph_o(n, p, seed);
    }

    if (connected) {
        if (wgh) return random_graph_w_c(n, p, seed, value_low, value_high);
        return random_graph_c(n, p, seed);
    }
    if (wgh) return random_graph_w(n, p, seed, value_low, value_high);
    return random_graph(n, p, seed);
}

void print_help() {
    std::cout
        << "Generate a random graph\n"
        << " -t            Number of graphs to generate\n"
        << " -n low high   Min and Max size of a graph\n"
        << " -p low high   Min and max probability of an edge\n"
        << " -v low high   Min and Max value for weighted graphs\n"
        << " -o            Oriented (directed) graph\n"
        << " -c            Connected (undirected) / weakly connected (directed)\n"
        << " -sc           Strongly connected (directed only; implies -o)\n"
        << " -s seed       Seed (0 => time seed)\n"
        << " -f file name  Defaults to graph.txt\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    std::vector<std::string> input;
    input.reserve(argc);
    for (int i = 0; i < argc; i++) input.push_back(std::string(argv[i]));


    std::string file_name("graph.txt");
    bool wgh = false;
    bool oriented = false;
    bool connected = false;
    bool strongly_connected = false;

    double seed = 0;
    int value_low = 0;
    int value_high = 0;
    double prob_low = 0;
    double prob_high = 1;
    int n_low = 0;
    int n_high = 100;
    int t = 1;

    for (int i = 1; i < argc; i++) {
        if (input[i].empty()) continue;
        if (input[i][0] != '-') continue;

        if (input[i] == "-t") {
            if (++i >= argc) { print_help(); return 1; }
            t = std::stoi(input[i]);
        } else if (input[i] == "-n") {
            if (i + 2 >= argc) { print_help(); return 1; }
            n_low = std::stoi(input[++i]);
            n_high = std::stoi(input[++i]);
        } else if (input[i] == "-v") {
            if (i + 2 >= argc) { print_help(); return 1; }
            value_low = std::stoi(input[++i]);
            value_high = std::stoi(input[++i]);
            wgh = true;
        } else if (input[i] == "-p") {
            if (i + 2 >= argc) { print_help(); return 1; }
            prob_low = std::stod(input[++i]);
            prob_high = std::stod(input[++i]);
        } else if (input[i] == "-o") {
            oriented = true;
        } else if (input[i] == "-c") {
            connected = true;
        } else if (input[i] == "-sc") {
            strongly_connected = true;
            oriented = true;
        } else if (input[i] == "-s") {
            if (++i >= argc) { print_help(); return 1; }
            seed = std::stod(input[i]);
        } else if(input[i] == "-h"){
            print_help();
        } else if(input[i] == "-f"){
            i++;
            file_name=std::string(input[i]);
        }
        else {
            print_help();
            return 1;
        }
    }

    PRNG64 RNG(seed);
    if (seed == 0) {
        RNG = PRNG64::time_seed();
    }

    std::ofstream f("graph.txt");
    if (!f) {
        std::cerr << "Cannot open graph.txt\n";
        return 1;
    }

    if (t > 1) f << t << "\n";

    for (int i = 0; i < t; i++) {
        auto G = generate_graph(wgh, oriented, connected, strongly_connected,
                                value_low, value_high,
                                prob_low, prob_high,
                                n_low, n_high,
                                RNG);

        for (int r = 0; r < G.size; r++) {
            for (int c = 0; c < G.size; c++) {
                f << G.matrix[r][c] << ' ';
            }
            f << "\n";
        }
    }
    std::cout<<"Generated!"<<"\n";
    return 0;
}
