#include "pandora/main.h"
#include <algorithm>
#include <iostream>

int main()
{
    //auto [item1, item2, item3] = pandora::getMultipleValues();
    auto[item1, item2, item3] = pandora::getMultipleValues();

    std::cout << "Items: " << item1 << ", " << item2 << ", " << item3 << std::endl;

    return 0;
}