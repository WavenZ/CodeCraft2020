#include <iostream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <atomic>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cassert>

#ifdef DEBUG
    #include "timer.h"
#endif

using namespace std;

const int nthread = 4;      // 线程数

const int maxs = 20000000;   // 最大环数
const int maxn = 2000000;    // 最大节点数

// 数据读取buffer
uint32_t loadbuf[nthread][maxn][3];                                                                  
uint32_t loadnum[nthread];

// 图数据结构（前向星）
struct Edge{
    uint32_t id;
    uint32_t money;
};

Edge From[maxn];        // 正图
Edge To[maxn];          // 反图
uint32_t phead[maxn];   // 各id起始位置
uint32_t plen[maxn];    // 各id转账数 
uint32_t qhead[maxn];
uint32_t qlen[maxn];

// 简易的固定长度vector，效率和数组相近
class Vector{
public:
    Vector(){ _data = new uint32_t[maxn]; }
    void push_back(uint32_t t){
        _data[_size++] = t;
    }
    int size(){ return _size; }
    void clear(){ _size = 0; }
    uint32_t* begin(){ return _data; }
    uint32_t* end(){ return _data + _size; }
    uint32_t& operator[](int index){ return _data[index]; }
private:
    uint32_t* _data;
    int _size;
};
Vector ID; //　真实ID表

// 简易的固定桶数hash_table，采用开放寻址法解决冲突
class Hash_map{
public:
    Hash_map(){
        map.resize(m);
    }
    void insert(uint32_t k){
        // 如果不存在则插入，否则直接返回，作用等同：
        // if(!Map.count(key)) Map.insert(key);
        int i = 0;
        int hash_val = 0;
        while (i < m) {
            hash_val = hash(k, i);
            if (map[hash_val].val == -1){
                map[hash_val].key = k;
                map[hash_val].val = ID.size();
                map[hash_val].len = 1;
                ID.push_back(k);
                hashes.push_back(hash_val);
                cnt++;
                break;
            }
            if(map[hash_val].key == k){
                map[hash_val].len++;
                break;
            }else i++;
        }
    }
    int search(uint32_t k){
        // 搜索
        int i = 0, hash_val = 0;
        while (i < m) {
            hash_val = hash(k, i);
            if(map[hash_val].val == -1) break;
            if(map[hash_val].key == k) return map[hash_val].val;
            else i++;
        }
        return -1;
    }
    void sort_hash(){
        // 将hash值排序，确保id映射前后相对大小不变化
        sort(hashes.begin(), hashes.end(), [&](int a, int b){
            return map[a].key < map[b].key;
        });
        for(int i = 0; i < hashes.size(); ++i){
            map[hashes[i]].val = i;
        }
    }
    int size(){
        return cnt;
    }
private:
    // 常数取质数，且 m1 < m
    const int m = 2222281;
    const int m1 = 2205167;
    int cnt = 0;
    // 数据结构
    struct data{
        uint32_t len = 0;
        uint32_t key;
        int val = -1;
    };
    vector<data> map;
    vector<int> hashes;
    uint32_t hash(uint32_t k, int i){
        // 哈希函数
        return k % m + i; // 一次哈希
        // return (k % m + i * (m1 - k % m1)) % m; // 双重哈希
    }
}Map;

inline bool check(const uint32_t& a, const uint32_t& b){
    return (b * 5UL >= a) && (a * 3UL >= b);
}


uint32_t* loops[nthread][5];    // 存3-7环
uint32_t loop_size[nthread];    // 存每个线程的总环数（不必要）
uint32_t loops_size[maxn][5];   // 存每个id的3-7环个数

uint8_t which_thread[maxn];     // 记录该id被哪个线程取到
uint8_t DIS[nthread][maxn];     // dfs辅助数组，用于记录逆向的最近距离

atomic<int> curNode;            // 原子计数，用于dfs负载均衡
void* find_thread(void* arg){
/*
    方法：６＋３
*/
#ifdef DEBUG
    Timer t;
#endif
    int tid = *(int*)arg;
    uint32_t* Money = new uint32_t[ID.size() + 1]; // 记录逆向１层的金额，用于减少第７层搜索
    memset(DIS[tid], 8, (ID.size() + 1) * sizeof(uint8_t));
    Vector reach;  // 记录逆向到达的节点，用于清空DIS数组
    uint32_t* loop3 = loops[tid][0];
    uint32_t* loop4 = loops[tid][1];
    uint32_t* loop5 = loops[tid][2];
    uint32_t* loop6 = loops[tid][3];
    uint32_t* loop7 = loops[tid][4];
    uint32_t from;
    uint32_t cnt3, cnt4, cnt5, cnt6, cnt7;
    uint32_t node, node1, node2, node3, node4, node5, node6;
    uint32_t money, money1, money2, money3, money4, money5, money6;
    while(1){
        // 获取任务
        node = curNode++;
        if(node >= ID.size()) break;
        which_thread[node] = tid;
        // 跳过出/入度为０的节点
        if(plen[node] == 0 || qlen[node] == 0){
            continue;
        }

        reach.clear();
        cnt3 = cnt4 = cnt5 = cnt6 = cnt7 = 0;
        // 逆向３层dfs，记录到起始点的最近距离
        for(int j = qhead[node]; j < qhead[node + 1]; ++j){
            const Edge& from1 = From[j];
            node1 = from1.id;
            if(node1 < node) break;
            money1 = from1.money;
            Money[node1] = money1;
            DIS[tid][node1] = 1;
            reach.push_back(node1);
            for(int k = qhead[node1]; k < qhead[node1 + 1]; ++k){
                const Edge& from2 = From[k];
                node2 = from2.id;
                money2 = from2.money;
                if(node2 < node) break;
                if(!check(money2, money1) || node2 == node) continue;
                else if(2 < DIS[tid][node2]){
                    DIS[tid][node2] = 2;
                    reach.push_back(node2);
                }
                for(int l = qhead[node2]; l < qhead[node2 + 1]; ++l){
                    const Edge& from3 = From[l];
                    node3 = from3.id;
                    money3 = from3.money;
                    if(node3 < node) break;
                    else if(!check(money3, money2) ||node3 == node ||  node3 == node1) continue;
                    if(3 < DIS[tid][node3]){
                        DIS[tid][node3] = 3;
                        reach.push_back(node3);
                    }
                }
            }
        }
        DIS[tid][node] = 0;
        // 正向６层dfs，循环中的判断顺序比较重要
        int j = phead[node];
        for(; j < phead[node + 1]; ++j) if(To[j].id > node) break;
        for(; j < phead[node + 1]; ++j){
            const Edge& to1 = To[j];
            node1 = to1.id;
            money1 = to1.money;
            int k = phead[node1];
            for(; k < phead[node1 + 1]; ++k) if(To[k].id > node) break;
            for(; k < phead[node1 + 1]; ++k){
                const Edge& to2 = To[k];
                node2 = to2.id;
                money2 = to2.money;
                if(!check(money1, money2)) continue;
                int l = phead[node2];
                for(; l < phead[node2 + 1]; ++l) if(To[l].id >= node) break;
                for(; l < phead[node2 + 1]; ++l){
                    const Edge& to3 = To[l];
                    node3 = to3.id;
                    money3 = to3.money;
                    if(!check(money2, money3) || node3 == node1) continue;
                    else if(node3 == node){
                        if(check(money3, money1)){
                            loop3[0] = node;loop3[1] = node1;loop3[2] = node2;
                            loop3 += 3;
                            cnt3++;
                        }
                        continue;
                    }
                    for(int m = phead[node3]; m < phead[node3 + 1]; ++m){
                        const Edge& to4 = To[m];
                        node4 = to4.id;
                        money4 = to4.money;
                        if(DIS[tid][node4] > 3 || !check(money3, money4)) continue;
                        else if(node4 == node){
                            if(check(money4, money1)){
                                loop4[0] = node;loop4[1] = node1;
                                loop4[2] = node2;loop4[3] = node3;
                                loop4 += 4;
                                cnt4++;
                            }
                            continue;
                        }else if(node4 == node1 || node4 == node2) continue;
                        for(int n = phead[node4]; n < phead[node4 + 1]; ++n){
                            const Edge& to5 = To[n];
                            node5 = to5.id;
                            money5 = to5.money;
                            if(DIS[tid][node5] > 2 || !check(money4, money5)) continue;
                            else if(node5 == node){
                                if(check(money5, money1)){
                                    loop5[0] = node;loop5[1] = node1;loop5[2] = node2;
                                    loop5[3] = node3;loop5[4] = node4;
                                    loop5 += 5;
                                    cnt5++;
                                }
                                continue;
                            }else if(node5 == node1 || node5 == node2 || node5 == node3) continue;
                            for(int o = phead[node5]; o < phead[node5 + 1]; ++o){
                                const Edge& to6 = To[o];
                                node6 = to6.id;
                                money6 = to6.money;
                                if(DIS[tid][node6] > 1 || !check(money5, money6)) continue;
                                else if(DIS[tid][node6] == 1){
                                    if(node6 == node1 || node6 == node2 || node6 == node3 || node6 == node4) continue;
                                    money = Money[node6];
                                    if(check(money6, money) && check(money, money1)){
                                        loop7[0] = node;loop7[1] = node1;loop7[2] = node2;loop7[3] = node3;
                                        loop7[4] = node4;loop7[5] = node5;loop7[6] = node6;
                                        loop7 += 7;
                                        cnt7++;
                                    }
                                }else if(node6 == node){
                                    if(check(money6, money1)){
                                        loop6[0] = node;loop6[1] = node1;loop6[2] = node2;
                                        loop6[3] = node3;loop6[4] = node4;loop6[5] = node5;
                                        loop6 += 6;
                                        cnt6++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        DIS[tid][node] = 8;
        for(int j = 0; j < reach.size(); ++j) DIS[tid][reach[j]] = 8; // 清空DIS数组
        // 统计
        loop_size[tid] = (loop3 - loops[tid][0]) / 3 +
                         (loop4 - loops[tid][1]) / 4 +
                         (loop5 - loops[tid][2]) / 5 +
                         (loop6 - loops[tid][3]) / 6 +
                         (loop7 - loops[tid][4]) / 7;
        loops_size[node][0] = cnt3;
        loops_size[node][1] = cnt4;
        loops_size[node][2] = cnt5;
        loops_size[node][3] = cnt6;
        loops_size[node][4] = cnt7;

    }
    *loop3 = *loop4 = *loop5 = *loop6 = *loop7 = 0xffffffff;
}

void find_loops(){
#ifdef DEBUG
    cout << __func__ << endl;
    Timer t;
#endif
    // 开内存用于存环
    for(int i = 0; i < nthread; ++i){
        for(int j = 0; j < 5; ++j){
            loops[i][j] = new uint32_t[maxs / 2 * (j + 3)];
        }
    }
    // 多线程找环
    pthread_t threads[nthread];
    int tid[nthread];
    for(int i = 0; i < nthread; ++i){
        tid[i] = i;
        pthread_create(&threads[i], NULL, find_thread, (void*)&tid[i]);
    }
    for(int i = 0; i < nthread; ++i)
        pthread_join(threads[i], NULL);
#ifdef DEBUG
    int loops_total = 0;
    for(int i = 0; i < nthread; ++i){
        loops_total += (loop_size[i]);
    }
    cout << "loops: " << loops_total << endl;
#endif
}

uint32_t* Loops[5]; // 合并的结果
size_t accu[8]; // 3-7环的累计和
void merge_loops(){
/*
    合并各线程找到的环
*/
#ifdef DEBUG
    cout << __func__ << endl;
    Timer t;
#endif
    // 开内存
    uint32_t* lptr[5];
    for(int i = 0; i < 5; ++i){
        Loops[i] = new uint32_t[maxs * (i + 3)];
        lptr[i] = Loops[i];
    }
    // 辅助指针
    uint32_t* ptr[nthread][5];
    for(int i = 0; i < nthread; ++i){
        for(int j = 0; j < 5; ++j){
            ptr[i][j] = loops[i][j];
        }
    }
    // 合并
    uint32_t len;
    for(int i = 0; i < 5; ++i){
        for(int j = 0; j < ID.size(); ++j){
            uint32_t* &curr = ptr[which_thread[j]][i]; // 从各个线程取出结果进行合并
            if(loops_size[j][i]){
                len = loops_size[j][i] * (i + 3);
                memcpy(lptr[i], curr, len * sizeof(uint32_t));
                lptr[i] += len;
                curr += len;
            }
        }
    }
    // 计算累计和
    for(int i = 3; i < 8; ++i){
        accu[i] = accu[i - 1] + (lptr[i - 3] - Loops[i - 3]) / i;
    }
}

inline void convert(uint32_t temp, char* ptr){
    // 转换：uint32 -> char[]
    // 仅在多线程建转换表时使用
    if(temp < 10){
        ptr[0] = 1;
        ptr[1] = temp % 10 + '0';
    }else if(temp < 100){
        ptr[0] = 2;
        ptr[1] = temp / 10 % 10 + '0';
        ptr[2] = temp % 10 + '0';
    }else if(temp < 1000){
        ptr[0] = 3;
        ptr[1] = temp / 100 % 10 + '0';
        ptr[2] = temp / 10 % 10 + '0';
        ptr[3] = temp % 10 + '0';
    }else if(temp < 10000){
        ptr[0] = 4;
        ptr[1] = temp / 1000 % 10 + '0';
        ptr[2] = temp / 100 % 10 + '0';
        ptr[3] = temp / 10 % 10 + '0';
        ptr[4] = temp % 10 + '0';
    }else if(temp < 100000){
        ptr[0] = 5;
        ptr[1] = temp / 10000 % 10 + '0';
        ptr[2] = temp / 1000 % 10 + '0';
        ptr[3] = temp / 100 % 10 + '0';
        ptr[4] = temp / 10 % 10 + '0';
        ptr[5] = temp % 10 + '0';
    }else if(temp < 1000000){
        ptr[0] = 6;
        ptr[1] = temp / 100000 % 10 + '0';
        ptr[2] = temp / 10000 % 10 + '0';
        ptr[3] = temp / 1000 % 10 + '0';
        ptr[4] = temp / 100 % 10 + '0';
        ptr[5] = temp / 10 % 10 + '0';
        ptr[6] = temp % 10 + '0';
    }else if(temp < 10000000){
        ptr[0] = 7;
        ptr[1] = temp / 1000000 % 10 + '0';
        ptr[2] = temp / 100000 % 10 + '0';
        ptr[3] = temp / 10000 % 10 + '0';
        ptr[4] = temp / 1000 % 10 + '0';
        ptr[5] = temp / 100 % 10 + '0';
        ptr[6] = temp / 10 % 10 + '0';
        ptr[7] = temp % 10 + '0';
    }else if(temp < 100000000){
        ptr[0] = 8;
        ptr[1] = temp / 10000000 % 10 + '0';
        ptr[2] = temp / 1000000 % 10 + '0';
        ptr[3] = temp / 100000 % 10 + '0';
        ptr[4] = temp / 10000 % 10 + '0';
        ptr[5] = temp / 1000 % 10 + '0';
        ptr[6] = temp / 100 % 10 + '0';
        ptr[7] = temp / 10 % 10 + '0';
        ptr[8] = temp % 10 + '0';
    }else if(temp < 1000000000){
        ptr[0] = 9;
        ptr[1] = temp / 100000000 % 10 + '0';
        ptr[2] = temp / 10000000 % 10 + '0';
        ptr[3] = temp / 1000000 % 10 + '0';
        ptr[4] = temp / 100000 % 10 + '0';
        ptr[5] = temp / 10000 % 10 + '0';
        ptr[6] = temp / 1000 % 10 + '0';
        ptr[7] = temp / 100 % 10 + '0';
        ptr[8] = temp / 10 % 10 + '0';
        ptr[9] = temp % 10 + '0';
    }else{
        ptr[0] = 10;
        ptr[1] = temp / 1000000000 % 10 + '0';
        ptr[2] = temp / 100000000 % 10 + '0';
        ptr[3] = temp / 10000000 % 10 + '0';
        ptr[4] = temp / 1000000 % 10 + '0';
        ptr[5] = temp / 100000 % 10 + '0';
        ptr[6] = temp / 10000 % 10 + '0';
        ptr[7] = temp / 1000 % 10 + '0';
        ptr[8] = temp / 100 % 10 + '0';
        ptr[9] = temp / 10 % 10 + '0';
        ptr[10] = temp % 10 + '0';
    }
}


char conv[maxn][11]; // 转换表
void* conv_thread(void* args){
    // 将映射后的id作为下标，真实id转换为字符串后保存在对应位置.
    // 例如: Map[5142132] = 42，则 conv[42] = "5142132";
    // 上述操作可以减少一次逆映射导致的访存
    int tid = *(int*)args;
    for(int i = tid; i < ID.size(); i += nthread){
        convert(ID[i], conv[i]);
    }
}
void convert_init(){
    pthread_t threads[nthread];
    int tid[nthread];
    for(int i = 0; i < nthread; ++i){
        tid[i] = i;
        pthread_create(&threads[i], NULL, conv_thread, (void*)&tid[i]);
    }
    for(int i = 0; i < nthread; ++i)
        pthread_join(threads[i], NULL);
}


char* buf[nthread];
int len[nthread];
void* itoa(void* arg){
    // 将结果转换为字符串

#ifdef DEBUG
    Timer t;
    // cout << __func__ << endl;
#endif
    int tid = *(int*)arg;
    // 任务分割，基本均衡
    int start = tid * (accu[7] / nthread);
    int end = (tid + 1) * (accu[7] / nthread);
    if(tid == nthread - 1) end = accu[7];

    // 查表转换
    int temp = 0, offset;
    char* ptr = buf[tid];
    for(int i = start; i < end; ++i){
        for(int j = 3; j < 8; ++j){ // 某些线程可能跨长度，典型为线程０跨越3-7环
            if(i < accu[j]){
                offset = (i - accu[j - 1]) * j;
                for(int k = 0; k < j; ++k){
                    temp = Loops[j - 3][offset + k];
                    char& sz = conv[temp][0];
                    memcpy(ptr, conv[temp] + 1, sz);
                    ptr += sz;
                    *ptr++ = ',';
                }
                *(ptr - 1) = '\n';
                break;
            }
        }
    }
    len[tid] = ptr - buf[tid];
}

// 保存结果：mmap + 多线程memcpy
// 线下看起来快，线上和fwrite差不多
char* write_pos;
void* save(void* arg){
    int tid = *(int*)arg;
    int start = 0;
    if(tid){
        for(int i = 0; i < tid; ++i) start += len[i];
    }
    memcpy(write_pos + start, buf[tid], len[tid]);
}

const char* test_data_file = "/data/test_data.txt";
const char* result_file = "/projects/student/result.txt";

void save_loops(){
// 保存结果
#ifdef DEBUG
    Timer t;
    cout << __func__ << endl;
#endif
    // 开内存
    for(int i = 0; i < nthread; ++i){
        buf[i] = new char[1024 * 1024 * 512];
    }
    // 构造转换表
    convert_init();
    // 结果转换为字符串
    pthread_t threads[nthread];
    int tid[nthread];
    for(int i = 0; i < nthread; ++i){
        tid[i] = i;
        pthread_create(&threads[i], NULL, itoa, (void*)&tid[i]);
    }

    for(int i = 0; i < nthread; ++i)
        pthread_join(threads[i], NULL);
    // 环数转换为字符串
    char* size_buf = new char[12];
    char* ptr = size_buf;
    uint32_t size = accu[7];
    convert(size, ptr);
    ptr += (*size_buf++ + 1);
    *ptr++ = '\n';
    // mmap写
    int total_len = (ptr - size_buf);
    for(int i = 0; i < nthread; ++i) total_len += len[i];
    int fd = open(result_file, O_RDWR | O_CREAT , 0666);
    lseek(fd, total_len -  1, SEEK_SET);
    int ret = write(fd, "\0", 1);
    char* ptr_ans = (char*)mmap(NULL, total_len, PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    // 写环数
    memcpy(ptr_ans, size_buf, ptr - size_buf);
    write_pos = ptr_ans + (ptr - size_buf);
    // 多线程写环
    for(int i = 0; i < nthread; ++i){
        pthread_create(&threads[i], NULL, save, (void*)&tid[i]);
    }
    for(int i = 0; i < nthread; ++i)
        pthread_join(threads[i], NULL);
}

void* sort_thread(void* arg){
    // 对每个节点的邻接表排序：正向正序（保证结果为字典序），反向反序（用于反向dfs提前break退出）
    int tid = *(int*)arg;
    for(int i = tid; i < ID.size(); i += nthread){
        sort(To + phead[i], To + phead[i] + plen[i], [](const Edge& a, const Edge& b){
            return a.id < b.id;
        });
        sort(From + qhead[i], From + qhead[i] + qlen[i], [](const Edge& a, const Edge& b){
            return a.id > b.id;
        });
    }
}

void* build_thread(void* arg){
    // 构造前向星
    // 1.统计每个节点邻接点个数
    // 2.计算每个节点开始位置
    // 3.遍历所有节点构造前向星
    int tid = *(int*)arg;
    uint32_t from, to, money;
    if(tid == 0){ // 正图
        int* curlen = new int[ID.size() + 1]();
        for(int k = 0; k < nthread; ++k){
            for(int i = 0; i < loadnum[k]; ++i){
                if(loadbuf[k][i][1] != 0xffffffff){
                    from = loadbuf[k][i][0];
                    plen[from]++;
                }
            }
        }
        phead[0] = 0;
        for(int i = 1; i <= ID.size(); ++i){
            phead[i] = phead[i - 1] + plen[i - 1];
        }
        for(int k = 0; k < nthread; ++k){
            for(int i = 0; i < loadnum[k]; ++i){
                if(loadbuf[k][i][1] != 0xffffffff){
                    to = loadbuf[k][i][1];
                    from = loadbuf[k][i][0];
                    To[phead[from] + curlen[from]].id = to;
                    To[phead[from] + curlen[from]++].money = loadbuf[k][i][2];
                }

            }
        }

    }else{ // 反图
        int* curlen = new int[ID.size() + 1]();
        for(int k = 0; k < nthread; ++k){
            for(int i = 0; i < loadnum[k]; ++i){
                if(loadbuf[k][i][1] != 0xffffffff){
                    to = loadbuf[k][i][1];
                    qlen[to]++;
                }
            }
        }
        qhead[0] = 0;
        for(int i = 1; i <= ID.size(); ++i){
            qhead[i] = qhead[i - 1] + qlen[i - 1];
        }
        for(int k = 0; k < nthread; ++k){
            for(int i = 0; i < loadnum[k]; ++i){
                if(loadbuf[k][i][1] != 0xffffffff){
                    to = loadbuf[k][i][1];
                    from = loadbuf[k][i][0];
                    From[qhead[to] + curlen[to]].id = from;
                    From[qhead[to] + curlen[to]++].money = loadbuf[k][i][2];
                }
            }
        }
    }
}

char* file;
int file_size;
void* load_thread(void* args){
    // 多线程读图
    int tid = *(int*)args;
    int size = file_size / nthread;
    if(tid == nthread - 1) size = file_size - (nthread - 1) * size;
    
    char* start = file + tid * (file_size / nthread);
    char* curr = start;

    // 确保两个线程不读到分割位置的同一行
    if(tid != 0 && *(curr - 1) != '\n') while(*curr++ != '\n');

    uint32_t from, to , money, temp = 0;
    uint8_t state = 0;
    char ch;
    while(1){
        ch = *curr;
        if(ch == ','){
            state ? to = temp : from = temp;
            state = 1 - state;
            temp = 0;
        }
        else if(ch == '\r' || ch == '\n'){
            loadbuf[tid][loadnum[tid]][0] = from;
            loadbuf[tid][loadnum[tid]][1] = to;
            loadbuf[tid][loadnum[tid]][2] = temp;
            loadnum[tid]++;
            if(ch == '\r') curr++;
            if(curr - start + 1 >= size) break;
            state = temp = 0;
        }
        else{
            temp = temp * 10 + ch - '0';
        }
        curr++;
    }
}

void* clear_thread(void* args){
    // 删除记录中出度为０的节点
    // 并将记录id映射为{ 0 １ ２ ... n-1 }
    int tid = *(int*)args;
    uint32_t from, to;
    for(int i = 0; i < loadnum[tid]; ++i){
        to = Map.search(loadbuf[tid][i][1]);
        if(to == -1){
            loadbuf[tid][i][1] = 0xffffffff; // 标记这一行转账数据不要了
        }else{
            from = Map.search(loadbuf[tid][i][0]);
            loadbuf[tid][i][0] = from;
            loadbuf[tid][i][1] = to;
        }
    }
}

void* shit_thread(void* args){
    // 哈希排序，确保映射前后相对大小不变
    int tid = *(int*)args;
    uint32_t from , to;
    if(tid == 0){
        Map.sort_hash();
    }else{
        sort(ID.begin(), ID.end());
    }
}

void load_data(){
/*
    @brief: load data from file "/data/test_data.txt"
    @method: mmap
*/
#ifdef DEBUG
    cout << __func__ << endl;
    Timer t;
#endif
    // mmap
    struct stat statue;
    int fd = open(test_data_file, O_RDONLY);
    fstat(fd, &statue);
    file_size = statue.st_size;
    file = (char*)mmap(NULL, statue.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    // 1.多线程读数据到loadbuf中
    pthread_t threads[nthread];
    int tid[nthread];
    for(int i = 0; i < nthread; ++i){
        tid[i] = i;
        pthread_create(&threads[i], NULL, load_thread, (void*)&tid[i]);
    }
    for(int i = 0; i < nthread; ++i)
        pthread_join(threads[i], NULL);

    // 2.哈希（仅映射 {u, v, w} 中的 u，因为不在 U 中出现的 id　肯定不成环）
    for(int k = 0; k < nthread; ++k){
        for(int i = 0; i < loadnum[k]; ++i){
                Map.insert(loadbuf[k][i][0]);
        }
    }
    // 3.哈希排序，确保映射前后相对大小不变
    for(int i = 0; i < 2; ++i){
        tid[i] = i;
        pthread_create(&threads[i], NULL, shit_thread, (void*)&tid[i]);
    }
    for(int i = 0; i < 2; ++i)
        pthread_join(threads[i], NULL);
 
    // 4.将不用的记录删除(使得后续不用再查询哈系表)
    for(int i = 0; i < nthread; ++i){
        tid[i] = i;
        pthread_create(&threads[i], NULL, clear_thread, (void*)&tid[i]);
    }
    for(int i = 0; i < nthread; ++i)
        pthread_join(threads[i], NULL);
 
    // 5.构造前向星
    for(int i = 0; i < 2; ++i){
        tid[i] = i;
        pthread_create(&threads[i], NULL, build_thread, (void*)&tid[i]);
    }
    for(int i = 0; i < 2; ++i)
        pthread_join(threads[i], NULL);

    // ６.前向星排序
    for(int i = 0; i < nthread; ++i){
        tid[i] = i;
        pthread_create(&threads[i], NULL, sort_thread, (void*)&tid[i]);
    }
    for(int i = 0; i < nthread; ++i)
        pthread_join(threads[i], NULL);
}

int main(int argc, char** argv){
#ifdef DEBUG
    Timer t;
#endif
    load_data();
    find_loops();
    merge_loops();
    save_loops();
#ifdef DEBUG
    cout << __func__ << endl;
#endif
    return 0;
}
