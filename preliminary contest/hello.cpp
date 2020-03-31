/*
    2020/3/31 by WavenZ
*/
#include <iostream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include "timer.h"

using namespace std;

const char* data_file = "test_data.txt";
const char* result_file = "result.txt";
const char* answer_file = "answer.txt";

struct Node{
    uint32_t from;
    vector<Node*> to;
    uint32_t amount;
    uint32_t state;
    uint16_t degree;
    Node(uint32_t f) : from(f), state(0), degree(0) {}
};

void read(unordered_map<uint32_t, Node*> &data){
    uint32_t from, to, amount;
    FILE* fp = fopen(data_file, "r");
    while(fscanf(fp, "%d,%d,%d", &from, &to, &amount) > 0){
        if(!data.count(from))
            data[from] = new Node(from);
        if(!data.count(to))
            data[to] = new Node(to);
        data[from]->from = from;
        data[from]->to.push_back(data[to]);
        data[from]->amount = amount;
        data[to]->degree ++;
    }
    fclose(fp);
}

void dfs(vector<vector<unsigned>> &loops, vector<unsigned> &loop, Node* node, int flag){
    if(node->state == 1){
        loops.push_back(vector<unsigned>(find(loop.begin(), loop.end(), node->from), loop.end()));
        if(loops.back().size() > 7 || loops.back().size() < 3) loops.pop_back();
    }
    if(node->state == 0 || node->state == flag){
        node->state = 1;
        loop.push_back(node->from);
        for(Node* to : node->to){
            dfs(loops, loop, to, flag);
        }
        loop.pop_back();
        node->state = flag;
    }
}

void search(unordered_map<uint32_t, Node*> &records, vector<vector<unsigned>> &loops){
    vector<unsigned> loop;
    int flag = 2;
    for(const auto &record : records){
        Node* node = record.second;
        if(node->state == 0 && node->degree > 0 && node->to.size()){
            dfs(loops, loop, node, flag++);
        }
    }
}

void sort_loops(vector<vector<unsigned>> &loops){
    for(vector<unsigned> &loop : loops){
        int min_index = min_element(loop.begin(), loop.end()) - loop.begin();
        vector<unsigned> temp = loop;
        for(int i = 0; i < loop.size(); ++i){
            loop[i] = temp[(min_index + i) % loop.size()];
        }
    }
    sort(loops.begin(), loops.end(), 
        [](const vector<unsigned>& a, const vector<unsigned>& b){
            if(a.size() != b.size()) return a.size() < b.size();
            else{
                for(int i = 0; i < a.size(); ++i){
                    if(a[i] < b[i]) return true;
                    if(a[i] > b[i]) return false;
                }
            }
            return true;
        });
}

void save_result(vector<vector<unsigned>> &loops){
    FILE* fp = fopen(result_file, "w");
    fprintf(fp, "%d\n", loops.size());
    for(const vector<unsigned>& loop : loops){
        for(int i = 0; i < loop.size() - 1; ++i){
            fprintf(fp, "%d,", loop[i]);
        }
        fprintf(fp, "%d\n", loop.back());
    }
}

int main(int argc, char* argv[]){
    Timer t;
    unordered_map<uint32_t, Node*> records;
    vector<vector<unsigned>> loops;
    read(records);
    search(records, loops);
    sort_loops(loops);
    save_result(loops);
    return 0;
}