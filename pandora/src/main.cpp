#include "pandora/main.h"
#include "ui/window.h"
#include <algorithm>
#include <iostream>

using namespace pandora;

int main()
{
    auto[item1, item2, item3] = pandora::getMultipleValues();
    std::cout << "Items: " << item1 << ", " << item2 << ", " << item3 << std::endl;
    
    Window myWindow(1280, 720, "Hello World!");
    while (!myWindow.shouldClose())
    {
    	myWindow.updateInput();
    	myWindow.swapBuffers();
    }

    return 0;
}