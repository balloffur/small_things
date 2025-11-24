#pragma once
#include <chrono>
#include <iostream>
static std::chrono::steady_clock::time_point begin;
static std::chrono::steady_clock::time_point end;
static bool is_timing=false;

void time(){
    if(is_timing){
        end=std::chrono::steady_clock::now();
        std::cout<<"Execution time:\n";
        std::cout<<std::chrono::duration_cast<std::chrono::milliseconds>(end-begin).count()<<"ms\n";
        std::cout<<std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count()<<"mсs\n";
        std::cout<<std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()<<"ns\n";
        is_timing=false;
    }
    else {
    is_timing=true;
    begin=std::chrono::steady_clock::now();
    }
}

namespace time_labels{
//Метка с подсчётом среднего
class time_label{
    private:
    std::chrono::steady_clock::time_point temp_begin=std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point global_begin=temp_begin;
    std::chrono::steady_clock::time_point temp_end;
    int64_t number_of_iterations=0;
    public:
    //Вывод в консоль времени с последней итерации
    void time(){
        temp_end=temp_begin;
        temp_begin=std::chrono::steady_clock::now();
        number_of_iterations++;
        std::cout<<"Execution time:\n";
        std::cout<<std::chrono::duration_cast<std::chrono::milliseconds>(temp_begin-temp_end).count()<<"ms\n";
        std::cout<<std::chrono::duration_cast<std::chrono::microseconds>(temp_begin-temp_end).count()<<"mсs\n";
        std::cout<<std::chrono::duration_cast<std::chrono::nanoseconds>(temp_begin-temp_end).count()<<"ns\n";
        temp_begin=std::chrono::steady_clock::now();
    }
    void tick(){
        number_of_iterations++;
    }
    //Среднее с начала метки
    void avarage(){
        temp_begin=std::chrono::steady_clock::now();
        std::cout<<"Avarage time over "<<number_of_iterations<<" iterations:\n";
        std::cout<<std::chrono::duration_cast<std::chrono::milliseconds>((temp_begin-global_begin)/number_of_iterations).count()<<"ms\n";
        std::cout<<std::chrono::duration_cast<std::chrono::microseconds>((temp_begin-global_begin)/number_of_iterations).count()<<"mсs\n";
        std::cout<<std::chrono::duration_cast<std::chrono::nanoseconds>((temp_begin-global_begin)/number_of_iterations).count()<<"ns\n";

        temp_begin=std::chrono::steady_clock::now();
    }
    //Перезапуск метки
    void reset(){
        number_of_iterations=0;
        temp_begin=std::chrono::steady_clock::now();
        global_begin=temp_begin;
    }
    //Пауза глобального и локального таймера
    void pause(){
    }
};
}
