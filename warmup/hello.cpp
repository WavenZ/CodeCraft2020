#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <vector>
#include <iostream>
#include<sys/wait.h>
#include <fcntl.h>
#include <sys/mman.h>   
#include <sys/stat.h>
#include <unistd.h>
#include <arm_neon.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstring>
#include <sys/stat.h>
#include <iomanip>
#include <pthread.h>
#include <chrono>


using namespace std;


// dataset capacity
// COLS是16的倍数
const int COLS = 176;
const int ROWS = 5000;
const int max_datasize = 5000;

float gain = 1.011 ;
int testBytes = 0;   // 测试文件总字节数

pthread_mutex_t mutex;
pthread_mutexattr_t mutexattr;

#define cpus 4

struct Share{
    int cnt;
    int num0;
    int num1;
    float mean0[COLS];
    float mean1[COLS];
};
Share* share;

const char* trainFile = "/data/train_data.txt";
const char* testFile = "/data/test_data.txt";
const char* resultFile = "/projects/student/result.txt";

char* train_buf, *test_buf;

void delay(int us){
    chrono::system_clock::time_point start = chrono::system_clock::now();
    while(1){
        if(chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count() > us) break;
    }
}
std::chrono::system_clock::time_point start;
void Merge_sum(float* sum, float* data){
    float32x4_t sum_vec, sum_vec1, sum_vec2, sum_vec3;
    float32x4_t data_vec, data_vec1, data_vec2, data_vec3;
    for(int i = 0; i < COLS; i += 16){
        sum_vec = vld1q_f32(sum + i);
        sum_vec1 = vld1q_f32(sum + i + 4);
        sum_vec2 = vld1q_f32(sum + i + 8);
        sum_vec3 = vld1q_f32(sum + i + 12);
        data_vec = vld1q_f32(data + i);
        data_vec1 = vld1q_f32(data + i + 4);
        data_vec2 = vld1q_f32(data + i + 8);
        data_vec3 = vld1q_f32(data + i + 12);
        sum_vec = vaddq_f32(sum_vec, data_vec);
        sum_vec1 = vaddq_f32(sum_vec1, data_vec1);
        sum_vec2 = vaddq_f32(sum_vec2, data_vec2);
        sum_vec3 = vaddq_f32(sum_vec3, data_vec3);
        vst1q_f32(sum + i, sum_vec);
        vst1q_f32(sum + i + 4, sum_vec1);
        vst1q_f32(sum + i + 8, sum_vec2);
        vst1q_f32(sum + i + 12, sum_vec3);
    }
}

void loadTrainData(int id){
    
    struct stat statue;
    int fd = open(trainFile, O_RDONLY);
    fstat(fd, &statue);

    train_buf = (char*)mmap(NULL, statue.st_size, PROT_READ, MAP_SHARED, fd, 0);
    char* curr = train_buf + id * max_datasize / cpus * 6500;
    while(*curr++ != '\n');
    
    float* sum0 = (float*)malloc(COLS * sizeof(float));
    float* sum1 = (float*)malloc(COLS * sizeof(float));
    float* data = (float*)malloc(COLS * sizeof(float));
    memset(sum0, 0, COLS * sizeof(float));
    memset(sum1, 0, COLS * sizeof(float));
    float number = 0.0f;
    int col_cnt = 0, row_cnt = 0; 
    int num0_cnt = 0, num1_cnt = 0;
    float A[3][58] = {
        {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f},
        {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f, 0.01f, 0.02f, 0.03f, 0.04f, 0.05f, 0.06f, 0.07f, 0.08f, 0.09f},
        {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f, 0.001f, 0.002f, 0.003f, 0.004f, 0.005f, 0.006f, 0.007f, 0.008f, 0.009f},
    };
    while(1){
        if(*curr == '-'){
            number = (curr[1]- '0') + A[0][curr[3]] + A[1][curr[4]];//  + A[2][curr[5]];
            number = -number;
        }else{
            number = (curr[0]- '0') + A[0][curr[2]] + A[1][curr[3]];//  + A[2][curr[4]];
        }
        data[col_cnt++] = number;
        if(col_cnt == COLS){ // 如果一条数据有了
            curr += (1000 - COLS) * 6;
            while(*curr++ != '\n');
            if(*(curr - 2) == '1'){
                num1_cnt ++;
                Merge_sum(sum1, data);
            }else{
                num0_cnt ++;
                Merge_sum(sum0, data);
            }
            row_cnt ++;
            if(row_cnt >= max_datasize / cpus) break;
            col_cnt = 0;
        }else{
            
            if(*curr == '-') curr += 7;
            else curr += 6; 
        }
    }

    pthread_mutex_lock(&mutex);
    share->num0 += num0_cnt;
    share->num1 += num1_cnt;
    float32x4_t mean0_vec, mean1_vec, sum0_vec, sum1_vec;
    // float32x4_t div_vec = vdupq_n_f32(1.0f / max_datasize);
    for(int i = 0; i < COLS; i += 4){
        mean0_vec = vld1q_f32(share->mean0 + i);
        mean1_vec = vld1q_f32(share->mean1 + i);
        sum0_vec = vld1q_f32(sum0 + i);
        sum1_vec = vld1q_f32(sum1 + i);
        mean0_vec = vaddq_f32(mean0_vec, sum0_vec);
        mean1_vec = vaddq_f32(mean1_vec, sum1_vec);
        vst1q_f32(share->mean0 + i, mean0_vec);
        vst1q_f32(share->mean1 + i, mean1_vec);
    }
    if(share->cnt == cpus - 1){ // 最后一个进程负责计算均值
        float32x4_t num0_vec = vdupq_n_f32(1.0f / share->num0);
        float32x4_t num1_vec = vdupq_n_f32(1.0f / share->num1);
        for(int i = 0; i < COLS; i += 4){
            mean0_vec = vld1q_f32(share->mean0 + i);
            mean1_vec = vld1q_f32(share->mean1 + i);
            mean0_vec = vmulq_f32(mean0_vec, num0_vec);
            mean1_vec = vmulq_f32(mean1_vec, num1_vec);
            vst1q_f32(share->mean0 + i, mean0_vec);
            vst1q_f32(share->mean1 + i, mean1_vec);
        }
        share->cnt = 0;
        }
    else share->cnt++;
    pthread_mutex_unlock(&mutex);
    free(sum0);
    free(sum1);
    free(data);
}

int predict_one(float* data){
    float32x4_t mean0_vec, mean1_vec, data_vec, temp0_vec, temp1_vec;
    float32x4_t mean0_vec1, mean1_vec1, data_vec1, temp0_vec1, temp1_vec1;
    float32x4_t mean0_vec2, mean1_vec2, data_vec2, temp0_vec2, temp1_vec2;
    float32x4_t mean0_vec3, mean1_vec3, data_vec3, temp0_vec3, temp1_vec3;
    float32x4_t err0_vec = vdupq_n_f32(0.0f);
    float32x4_t err1_vec = vdupq_n_f32(0.0f);
    for(int i = 0; i < COLS; i += 16){
        mean0_vec = vld1q_f32(share->mean0 + i);
        mean1_vec = vld1q_f32(share->mean1 + i);
        data_vec = vld1q_f32(data + i);
        temp0_vec = vsubq_f32(data_vec, mean0_vec);
        temp1_vec = vsubq_f32(data_vec, mean1_vec);
        err0_vec = vmlaq_f32(err0_vec, temp0_vec, temp0_vec);
        err1_vec = vmlaq_f32(err1_vec, temp1_vec, temp1_vec);

        mean0_vec1 = vld1q_f32(share->mean0 + i + 4);
        mean1_vec1 = vld1q_f32(share->mean1 + i + 4);
        data_vec1 = vld1q_f32(data + i + 4);
        temp0_vec1 = vsubq_f32(data_vec1, mean0_vec1);
        temp1_vec1 = vsubq_f32(data_vec1, mean1_vec1);
        err0_vec = vmlaq_f32(err0_vec, temp0_vec1, temp0_vec1);
        err1_vec = vmlaq_f32(err1_vec, temp1_vec1, temp1_vec1);

        mean0_vec2 = vld1q_f32(share->mean0 + i + 8);
        mean1_vec2 = vld1q_f32(share->mean1 + i + 8);
        data_vec2 = vld1q_f32(data + i + 8);
        temp0_vec2 = vsubq_f32(data_vec2, mean0_vec2);
        temp1_vec2 = vsubq_f32(data_vec2, mean1_vec2);
        err0_vec = vmlaq_f32(err0_vec, temp0_vec2, temp0_vec2);
        err1_vec = vmlaq_f32(err1_vec, temp1_vec2, temp1_vec2);

        mean0_vec3 = vld1q_f32(share->mean0 + i + 12);
        mean1_vec3 = vld1q_f32(share->mean1 + i + 12);
        data_vec3 = vld1q_f32(data + i + 12);
        temp0_vec3 = vsubq_f32(data_vec3, mean0_vec3);
        temp1_vec3 = vsubq_f32(data_vec3, mean1_vec3);
        err0_vec = vmlaq_f32(err0_vec, temp0_vec3, temp0_vec3);
        err1_vec = vmlaq_f32(err1_vec, temp1_vec3, temp1_vec3);
    }
    float32x2_t r0 = vadd_f32(vget_high_f32(err0_vec), vget_low_f32(err0_vec));
    float err0 = vget_lane_f32(vpadd_f32(r0, r0), 0);
    float32x2_t r1 = vadd_f32(vget_high_f32(err1_vec), vget_low_f32(err1_vec));
    float err1 = vget_lane_f32(vpadd_f32(r1, r1), 0);
    return err0 * gain > err1;
}

char* write_buf;
int test_size;
void loadTestDataAndPredict(int id){

    struct stat statue;
    int fd = open(testFile, O_RDONLY);
    fstat(fd, &statue);
    test_buf = (char*)mmap(NULL, statue.st_size, PROT_READ, MAP_SHARED, fd, 0);
    char* curr = test_buf + id * 20000 * 6000 / cpus;
    
    int size = 5000;
    if(share->num0 > share->num1) gain = 1.0 / gain;
    float* data = (float*)malloc(COLS * sizeof(float));
    float A[3][58] = {
        {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f},
        {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f, 0.01f, 0.02f, 0.03f, 0.04f, 0.05f, 0.06f, 0.07f, 0.08f, 0.09f},
        {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f, 0.001f, 0.002f, 0.003f, 0.004f, 0.005f, 0.006f, 0.007f, 0.008f, 0.009f},
    };
    write_buf = (char*)malloc(42000 / cpus * sizeof(char));
    memset(write_buf, '\n', 42000 / cpus * sizeof(char));
    for(int i = 0; i < size; ++i){
        for(int j = 0; j < COLS; j ++){
            data[j] = curr[0] - '0' + A[0][curr[2]] + A[1][curr[3]];//  + A[2][curr[4]];;
            curr += 6;
        }
        write_buf[2 * i] = predict_one(data) ? '1' : '0';
        curr += (1000 - COLS) * 6;
    }
    test_size = size;
    free(data);
}

void accuracy(){
    FILE* fp1 = fopen("answer.txt", "r");
    FILE* fp2 = fopen(resultFile, "r");
    if(fp1 == nullptr || fp2 == nullptr){
        cout << "open failed." << endl;
        return;
    }
    int a, b, cnt = 0, total = 0, cntp = 0, cntn = 0;
    while(fscanf(fp1, "%d", &a) != EOF && fscanf(fp2, "%d", &b) != EOF){
        if(a == b) cnt++;
        else if(a == 1) cntp++;
        else cntn++;
        total++;
    }
    cout << setprecision(4);
    cout << "Acc: " << cnt * 1.0f / total << " " << cntp << " " << cntn << endl;
    fclose(fp1);
    fclose(fp2);
}
int main(){
    start = chrono::system_clock::now();
    
    // 2. 创建均值的共享内存
    int shmid = shmget((key_t)823, sizeof(Share), 0666 | IPC_CREAT);
    share = (Share*)shmat(shmid, 0, 0);

    share->cnt = 0;
    share->num0 = 0;
    share->num1 = 0;
    memset(share->mean0, 0, COLS * sizeof(float));
    memset(share->mean1, 0, COLS * sizeof(float));

    // 3. 创建互斥锁
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&mutex, &mutexattr);
#ifdef DEBUG
    cout << "fork start: "<< chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count() << endl;
#endif
    // 4. 创建进程
    int id;
    for(id = 0; id < cpus; ++id){
        pid_t pid = fork();
        if(pid == 0){ // 子进程
#ifdef DEBUG
            cout << "process " << id << "train start: "<< chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count() << endl;
#endif
            loadTrainData(id);
#ifdef DEBUG
            cout << "process " << id << "train finish: "<< chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count() << endl;
#endif
            while(1){
                if(share->cnt == 0) break;
                delay(1);
            }
#ifdef DEBUG
            cout << "process " << id << "predict start: "<< chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count() << endl;
#endif
            loadTestDataAndPredict(id);
#ifdef DEBUG
            cout << "process " << id << "predict finish: "<< chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count() << endl;
#endif
            while(1){
                if(share->cnt == id) break;
                delay(1);
            }
#ifdef DEBUG
            cout << "process " << id << "write start: "<< chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count() << endl;
#endif
            FILE* fp;
            if(id == 0)
                fp = fopen(resultFile, "w");
            else
                fp = fopen(resultFile, "a");
            fwrite(write_buf, 1, 2 * test_size * sizeof(char), fp);
            free(write_buf);
            fclose(fp);
#ifdef DEBUG
            cout << "process " << id << "write finish: "<< chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count() << endl;
#endif
            pthread_mutex_lock(&mutex);
            share->cnt ++;
            pthread_mutex_unlock(&mutex);

            usleep(1000);
            exit(0);
        }
    }
#ifdef DEBUG
    cout << "fork finish: "<< chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count() << endl;
#endif
    while(1){
        if(share->cnt == cpus) break;
        usleep(100);
    }
    // accuracy();
    // 4. 销毁共享内存
    shmdt(share);
    shmctl(shmid, IPC_RMID, 0);
    return 0;
}