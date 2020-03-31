#pragma once
#include <iostream>
#include <chrono>
#include <iomanip>
class Timer{
public:
    Timer(){
        start_time = std::chrono::system_clock::now();
    }
    ~Timer(){
        if(!end_flag) end();
    }
    void start(){
        start_time = std::chrono::system_clock::now();
    }
    void end(){
        end_time = std::chrono::system_clock::now();
        int cost = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        std::cout << std::setprecision(4);
        if(cost < 1000){                // us
            std::cout << "Time cost: " << cost << " us" << std::endl;
        }else if(cost < 1000000){       // ms
            std::cout << "Time cost: " << cost / 1000.0 << " ms" << std::endl;
        }else if(cost < 60000000){    // s
            std::cout << "Time cost: " << cost / 1000000.0 << " s" << std::endl;
        }else{                          // min
            std::cout << "Time cost: " << cost / 60000000 << " min "
                                       << cost % 60000000 / 1000000 << " s" << std::endl;
        }
        end_flag = true;
    }
private:
    bool end_flag = false;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
};