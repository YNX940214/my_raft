#include <iostream>
#include "ThreadPool.h"
#include "../util.h"

void func(int i) {
    Log_debug << "thread " << i << "begins";
//    std::cout << "thread " << i << " begins, sleep ";
    std::this_thread::sleep_for(std::chrono::milliseconds(i * 1000));
    Log_debug << "thread " << i << " ends " << std::endl;
//    std::cout << "thread " << i << " ends " << std::endl;

}

int main() {
    ThreadPool pool(2);
    for (int i = 0; i < 2; i++) {
        pool.enqueue([i]() {
            func(i + 1);
        });
    }
}
