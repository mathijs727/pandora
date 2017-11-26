#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string_view>

namespace pandora
{

class Window
{
public:
    Window(int width, int height, std::string_view title);
    ~Window();

    bool shouldClose();// Whether the user clicked on the close button
    void updateInput();
    void swapBuffers();// Swap the front/back buffer
private:
    GLFWwindow *m_window;
};
}