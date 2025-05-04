#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <stack>
#include <math.h>
#include <unordered_set>
#include <climits>
#include <queue>
#include <algorithm>

using namespace std;
#define MAX 10000

struct Edge {
    int dest;
    int weight;
};

using Graph = vector<vector<Edge>>;
int numVertices = 0;

void removeEdge(Graph &graph, int u, int v) {
    graph[u].erase(remove_if(graph[u].begin(), graph[u].end(),
                             [v](const Edge &edge) { return edge.dest == v; }),
                   graph[u].end());
    graph[v].erase(remove_if(graph[v].begin(), graph[v].end(),
                             [u](const Edge &edge) { return edge.dest == u; }),
                   graph[v].end());
}

void addEdge(Graph &graph, int src, int dest, int weight) {
    int yes = 0;
    for(auto &edge:graph[src]){
        if(edge.dest == dest){
            edge.weight = weight;
            yes = 1;
            break;
        }
    }
    for(auto &edge:graph[dest]){
        if(edge.dest == src){
            edge.weight = weight;
            break;
        }
    }
    if(yes==0){
        graph[src].push_back({dest,weight});
        graph[dest].push_back({src,weight});
    }
    
}
vector<int> findPath(int src, int dest, const vector<vector<int>> &prev) {
    vector<int>tmp;
    while(1){
        if (prev[src][dest] == -1) {
            tmp.push_back(dest);
            return tmp;
        }
        tmp.push_back(dest);
        dest = prev[src][dest];
    }
}
vector<int> bellman(const Graph &graph, int V, int start, vector<int> &prev) {
    vector<int> dist(V + 1, INT_MAX);
    dist[start] = 0;
    vector<int> nextHop(V + 1, -1);
    //nextHop.assign(V + 1, -1);
    prev.assign(V + 1, -1);

    for (int i = 0; i < V - 1; ++i) {
        for (int u = 1; u <= V; ++u) {
            for (const auto &edge : graph[u]) {
                int v = edge.dest;
                int weight = edge.weight; 
                if (dist[u] != INT_MAX) {
                    if (dist[u] + weight < dist[v]) {
                        dist[v] = dist[u] + weight;
                        prev[v] = u;
                        nextHop[v] = (u == start) ? v : nextHop[u];
                    }
                    else if (dist[u] + weight == dist[v]) {
                        int currentNextHop = (prev[v] == start) ? v : nextHop[v];
                        int newNextHop = (u == start) ? v : nextHop[u];

                        if (newNextHop < currentNextHop) {
                            prev[v] = u;
                            nextHop[v] = newNextHop;
                        }
                    }
                }
            }
        }
    }
    return dist;
}

void printForwardingTable(FILE* fpOut, vector<int> &prev, int start, const vector<int> &dist) {
    for (int i = 1; i < prev.size(); ++i) {
        if(dist[i]!=INT_MAX){
            int next_hop = i;
            while (prev[next_hop] != start && prev[next_hop] != -1) {
                next_hop = prev[next_hop];
            }
            fprintf(fpOut, "%d %d %d\n", i, next_hop, dist[i]);
        }
    }
}

void createTopo(ifstream& topo, Graph &graph){
    int src, dest, cost;
    string line;
    while(getline(topo, line)){
        istringstream iss(line);
        iss >> src >> dest >> cost;
        numVertices = max(max(src, dest), numVertices);
        graph[src].push_back({dest,cost});
        graph[dest].push_back({src,cost});
    }
}

void printMessage(FILE* fpOut,string file, vector<vector<int>> &prev, vector<vector<int>> &distance) {
    ifstream message(file);
    string line;
    string msg;
    while(getline(message, line)){
        int src, dest;
        istringstream iss(line);
        iss >> src >> dest;
        msg = line.substr(line.find(" "));
        msg = msg.substr(line.find(" ") + 1);

        if(distance[src][dest] == INT_MAX){
            fprintf(fpOut, "from %d to %d cost infinite hops unreachable message %s\n", src, dest, msg.c_str());
            continue;
        }
        vector<int> path = findPath(src, dest, prev);
        path.push_back(src); 
        fprintf(fpOut, "from %d to %d cost %d hops ", src, dest, distance[src][dest]);
        for (int i = path.size() - 2; i >= 0; --i) {
            if (path[i] == dest) continue;
            fprintf(fpOut, "%d ", path[i]);
        }
        fprintf(fpOut, "message %s\n", msg.c_str());

    }
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }
    ifstream topo(argv[1]);
    //ifstream message(argv[2]);
    ifstream change(argv[3]);
    Graph graph(MAX);
    createTopo(topo, graph);

    vector<vector<int>> distance(numVertices+1, vector<int>(numVertices+1, 0));
    vector<vector<int>> prevs(numVertices+1, vector<int>(numVertices+1, 0));
    //vector<vector<int>> nextHops(numVertices+1, vector<int>(numVertices+1, 0));

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    for(int i=1;i<=numVertices;i++){
        vector<int> prev;
        vector<int> dist = bellman(graph, numVertices, i, prev);
        printForwardingTable(fpOut, prev, i, dist);
        distance[i] = dist;
        prevs[i] = prev;
    }
    printMessage(fpOut, argv[2], prevs, distance);

    int src,dest,cost;
    while (change >> src >> dest >> cost){
        fstream message(argv[2]);
        if(cost == -999){
            removeEdge(graph,src,dest);
        }
        else{
            addEdge(graph,src,dest,cost);
        }
        for(int i=1;i<=numVertices;i++){
            vector<int> prev;
            vector<int> dist = bellman(graph, numVertices, i, prev);
            printForwardingTable(fpOut, prev, i, dist);
            distance[i] = dist;
            prevs[i] = prev;
        }
        printMessage(fpOut, argv[2], prevs, distance);
    }
    
    fclose(fpOut);

    return 0;
}

