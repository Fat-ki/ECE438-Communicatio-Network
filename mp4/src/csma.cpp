#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include <math.h>
using namespace std;
typedef struct{
    int backoff;
    int collision;
}node;

int num_nodes;
int length;
int max_tries;
vector<int> random_range;
double ticks;
double total_used = 0;

int calculateBackoff(int ID, int t, int R){
    return (ID+t) % (random_range[R]);
}

void printTable(vector<node*>& nodes){
    for(int i = 0; i < nodes.size(); i++){
        cout << i << ":" << nodes[i]->backoff <<" ";
    }
    cout << endl;
}

void printCollide(vector<node*>& nodes){
    for(int i = 0; i < nodes.size(); i++){
        cout << i << ":" << nodes[i]->collision <<" ";
    }
    cout << endl;
}

vector<int> findNodes(vector<node*>& nodes){
    vector<int> free_nodes;
    for(int i = 0; i < nodes.size(); i++){
        if(nodes[i]->backoff == 0){
            free_nodes.push_back(i);
        }
    }
    return free_nodes;
}


void setParam(string inputFile){
    char c;
    ifstream file(inputFile);

    string line;
    while (getline(file, line)) {
        stringstream ss(line);
        string key;
        ss >> key;

        if (key == "N") {
            ss >> num_nodes;
        } else if (key == "L") {
            ss >> length;
        } else if (key == "M") {
            ss >> max_tries;
        } else if (key == "T") {
            ss >> ticks;
        } else if (key == "R") {
            int r_value;
            while (ss >> r_value) {
                random_range.push_back(r_value);
            }
        }
    }
    file.close();
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }
    double finalTime = 0;
    setParam(argv[1]);
    vector<node*> nodes;
    //initiate
    for(int i=0;i<num_nodes;i++){
        node* n = new node();
        n->backoff = i % random_range[0];
        n->collision = 0;
        nodes.push_back(n);
    }
    //printTable(nodes);
    for(int i=0;i<ticks;i++){
        vector<int> nodesToSend = findNodes(nodes);
        int numSend = nodesToSend.size();

        if(numSend == 0){
            for(int j=0;j<nodes.size();j++){
                nodes[j]->backoff--;
            }
        }
        else if(numSend == 1){
            //printTable(nodes);
            nodes[nodesToSend[0]]->collision = 0;
            nodes[nodesToSend[0]]->backoff = (nodesToSend[0]+i+length) % random_range[nodes[nodesToSend[0]]->collision];
            if (i + length <= ticks) {
                // if (i == 0) i = length;
                // else i += length - 1;
                i+=length-1;
                total_used += length;
            }
            else{
                total_used += ticks-i;
                i = ticks;
            }
        }
        else{
            for (int j = 0; j < numSend; j++) {
                nodes[nodesToSend[j]]->collision++;
                if (nodes[nodesToSend[j]]->collision == max_tries) {
                    nodes[nodesToSend[j]]->collision = 0;
                }
                nodes[nodesToSend[j]]->backoff = (nodesToSend[j]+i+1) % random_range[nodes[nodesToSend[j]]->collision];
            }
        }
        //printTable(nodes);
    }
    //cout<<finalTime<<endl;
    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    double res = (total_used+finalTime) / ticks;
    res = round(res * 100.0) / 100.0;
    fprintf(fpOut, "%.2f\n", res);
    fclose(fpOut);

    return 0;
}

