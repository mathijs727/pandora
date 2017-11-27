#include "pandora/core/sensor.h"
#include "ui/framebuffer_gl.h"
#include "ui/window.h"
#include <algorithm>
#include <iostream>

using namespace pandora;
using namespace atlas;

const int width = 1280;
const int height = 720;

int main()
{
    Sensor sensor(width, height);
    sensor.clear(Vec3f(0.3f, 0.5f, 0.8f));

    Window myWindow(width, height, "Hello World!");
    FramebufferGL frameBuffer;
    frameBuffer.clear(Vec3f(0.8f, 0.5f, 0.3f));
    frameBuffer.update(sensor);
    while (!myWindow.shouldClose()) {
        myWindow.updateInput();
        myWindow.swapBuffers();
    }

    return 0;
}