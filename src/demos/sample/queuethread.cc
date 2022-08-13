#include "threadctl.h"

#include <iostream>
#include <vector>
#include <chrono>

#include <stdint.h>

int32_t main()
{
    auto func =[](const int32_t& data) { std::cout << "data " << data << std::endl; };
    CQueueThread<int32_t> queue_thread(3, func);
    queue_thread.start();

    for(int32_t i = 0; i < 800; ++i) {
        queue_thread.enqueue(i);
    }

    return 0;
}