#include "hello_world.h"
#include "tbb/tbb.h"
#include <iostream>
#include <memory>

void helloWorld()
{
    std::cout << "Hello world!" << std::endl;
    tbb::parallel_for(0, 10, [](int i) {
        std::cout << i << std::endl;
    });
    std::cout << "Still alive!" << std::endl;
}