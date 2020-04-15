/*
    2020/3/31 by WavenZ
*/
#include <iostream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstdio>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <cstdlib>


using namespace std;

const char* data_file = "/data/test_data.txt";
const char* result_file = "/projects/student/result.txt";

struct Node{
    uint32_t from;
    vector<Node*> to;
    uint32_t visit;
    uint16_t degree;
    Node(uint32_t f) : from(f), visit(0), degree(0) {}
};

vector<Node*> trans; // 交易
unordered_map<uint32_t, Node*> Map; // 账号映射为连续值
vector<vector<uint32_t>> Res; // 结果

void sort_trans(vector<Node*>& trans){ // 将交易按字典序排序
    sort(trans.begin(), trans.end(), [](const Node* a, const Node* b){
        return a->from < b->from;
    });
    for(int i = 0; i < trans.size(); ++i){
        sort(trans[i]->to.begin(), trans[i]->to.end(), [](const Node* a, const Node* b){
            return a->from < b->from;
        });
    }
}

void read(){ // 读取数据　
    struct stat statue;
    int fd = open(data_file, O_RDONLY);
    fstat(fd, &statue);
    int size = statue.st_size;
    char* start = (char*)mmap(NULL, statue.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    char* curr = start;
    uint32_t from, to, amount, number = 0;;
    uint8_t state = 0;
    char ch;
    while(1){
        ch = *curr;
        if(ch == ','){
            state ? to = number : from = number;
            state = 1 - state;
            number = 0;
        }
        else if(ch == '\n'){
            amount = number;
            if(!Map.count(from)) Map[from] = new Node(from);
            if(!Map.count(to)) Map[to] = new Node(to);
            Map[from]->to.push_back(Map[to]);
            Map[to]->degree ++;
            if(curr - start + 1 >= size) break;
            state = number = 0;
        }
        else{
            number = number * 10 + ch - '0';
        }
        curr++;
    }
    for(const auto& m : Map){
        trans.push_back(m.second);
    }
    sort_trans(trans);
}

void solve(){
    // ７层dfs算法
    vector<unsigned> res;
    int cnt = 0;
    for(int i = 0; i < trans.size(); i ++){
        Node* node = trans[i];
        if(node->degree > 0){
            node->visit = 1;
            res.push_back(node->from);
            for(Node* node1 : node->to){
                if(node1->from > node->from){
                    node1->visit = 1;
                    res.push_back(node1->from);
                    for(Node* node2 : node1->to){
                        if(node2->from > node->from){
                            node2->visit = 1;
                            res.push_back(node2->from);
                            for(Node* node3 : node2->to){
                                if(node3->from == node->from){
                                    Res.push_back(res);
                                }   
                                else if(node3->from < node->from || node3->visit == 1) continue;
                                else{
                                    node3->visit = 1;
                                    res.push_back(node3->from);
                                    for(Node* node4 : node3->to){
                                        if(node4->from == node->from){
                                            Res.push_back(res);
                                        }
                                        else if(node4->from < node->from || node4->visit == 1) continue;
                                        else{
                                            node4->visit = 1;
                                            res.push_back(node4->from);
                                            for(Node* node5 : node4->to){
                                                if(node5->from == node->from){
                                                    Res.push_back(res);
                                                }
                                                else if(node5->from < node->from || node5->visit == 1) continue;
                                                else{
                                                    node5->visit = 1;
                                                    res.push_back(node5->from);
                                                    for(Node* node6 : node5->to){
                                                        if(node6->from == node->from){
                                                            Res.push_back(res);
                                                        }
                                                        else if(node6->from < node->from || node6->visit == 1) continue;
                                                        else{
                                                            node6->visit = 1;
                                                            res.push_back(node6->from);
                                                            for(Node* node7 : node6->to){
                                                                if(node7->from == node->from){
                                                                    Res.push_back(res);
                                                                }
                                                            }
                                                            node6->visit = 0;
                                                            res.pop_back();
                                                        }
                                                    }
                                                    node5->visit = 0;
                                                    res.pop_back();
                                                }
                                            }
                                            node4->visit = 0;
                                            res.pop_back();
                                        }
                                    }
                                    node3->visit = 0;
                                    res.pop_back();
                                }
                            }
                            node2->visit = 0;
                            res.pop_back();
                        }
                    }
                    node1->visit = 0;
                    res.pop_back();
                }
            }
            node->visit = 0;
            res.pop_back();
        }
    }
}

void sort_res(){
    // 按长度排序
    stable_sort(Res.begin(), Res.end(), 
        [](const vector<unsigned>& a, const vector<unsigned>& b){
                return a.size() < b.size();
    });
}

void save_res(){
    // 保存结果
    FILE* fp = fopen(result_file, "w");
    fprintf(fp, "%d\n", (int)Res.size());
    for(int i = 0; i < Res.size(); i ++){
        for(int j = 0; j < Res[i].size() - 1; ++j){
            fprintf(fp, "%d,", Res[i][j]);
        }
        fprintf(fp, "%d\n", Res[i].back());
    }
}

int main(int argc, char* argv[]){
    read();
    solve();
    sort_res();
    save_res();
    return 0;
}