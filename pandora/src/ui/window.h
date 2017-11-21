#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace pandora
{

class Window
{
public:
    Window(int width, int height, const char *title);
    ~Window();

    bool shouldClose();// Whether the user clicked on the close button
    void updateInput();
    void swapBuffers();// Swap the front/back buffer
private:
    GLFWwindow *m_window;
};
}