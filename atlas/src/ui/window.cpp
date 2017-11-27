#include "ui/window.h"
#include <iostream>

namespace atlas {

void errorCallback(int error, const char* description)
{
    std::cerr << "GLFW error code: " << error << std::endl;
    std::cerr << description << std::endl;
    exit(1);
}

Window::Window(int width, int height, gsl::cstring_span<> title)
{
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW" << std::endl;
        exit(1);
    }

    glfwSetErrorCallback(errorCallback);
    m_window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
    if (m_window == nullptr) {
        glfwTerminate();
        std::cerr << "Could not create GLFW window" << std::endl;
        exit(1);
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(0);

    glewInit();
}

Window::~Window()
{
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool Window::shouldClose()
{
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::updateInput()
{
    glfwPollEvents();
}

void Window::swapBuffers()
{
    glfwSwapBuffers(m_window);
}
}