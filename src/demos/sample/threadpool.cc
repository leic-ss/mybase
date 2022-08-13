#include "threadctl.h"

#include <iostream>
#include <vector>
#include <chrono>

#include <stdint.h>

int32_t main()
{
    CThreadPool pool(4);

    for(int32_t i = 0; i < 8; ++i) {
        pool.execute([i] {
            std::cout << "hello " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "world " << i << std::endl;
            return ;
        });
    }
    
    return 0;
}