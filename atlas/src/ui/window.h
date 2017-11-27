#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <gsl/gsl>

namespace atlas {

class Window {
public:
    Window(int width, int height, gsl::cstring_span<> title);
    ~Window();

    bool shouldClose(); // Whether the user clicked on the close button
    void updateInput();
    void swapBuffers(); // Swap the front/back buffer
private:
    GLFWwindow* m_window;
};
}