#include "ui/window.h"
#include <iostream>

namespace atlas {

void errorCallback(int error, const char* description)
{
    std::cerr << "GLFW error code: " << error << std::endl;
    std::cerr << description << std::endl;
    exit(1);
}

Window::Window(int width, int height, std::string_view title)
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

    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, mouseMoveCallback);

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

void Window::registerKeyCallback(KeyCallback&& callback)
{
    m_keyCallbacks.push_back(std::move(callback));
}

void Window::registerMouseButtonCallback(MouseButtonCallback&& callback)
{
    m_mouseButtonCallbacks.push_back(std::move(callback));
}

void Window::registerMouseMoveCallback(MouseMoveCallback&& callback)
{
    m_mouseMoveCallbacks.push_back(std::move(callback));
}

void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Window* thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));

    for (auto& callback : thisWindow->m_keyCallbacks)
        callback(key, scancode, action, mods);
}

void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    Window* thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));

    for (auto& callback : thisWindow->m_mouseButtonCallbacks)
        callback(button, action, mods);
}

void Window::mouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
    Window* thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));

    for (auto& callback : thisWindow->m_mouseMoveCallbacks)
        callback(Vec2d(xpos, ypos));
}

void Window::setMouseCapture(bool capture)
{
    if (capture) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    glfwPollEvents();
}
}
