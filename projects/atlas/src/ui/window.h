#pragma once
#include "GL/glew.h" // Include before glfw3
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include <functional>
#include <string_view>
#include <vector>

namespace atlas {

class Window {
public:
    Window(int width, int height, std::string_view title);
    ~Window();

    bool shouldClose(); // Whether the user clicked on the close button
    void updateInput();
    void swapBuffers(); // Swap the front/back buffer

    using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
    void registerKeyCallback(KeyCallback&&);

    using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
    void registerMouseButtonCallback(MouseButtonCallback&&);

    using MouseMoveCallback = std::function<void(glm::dvec2 newPosition)>;
    void registerMouseMoveCallback(MouseMoveCallback&&);
   
    bool isKeyDown(int key);

    void setMouseCapture(bool capture);

private:
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos);

private:
    GLFWwindow* m_window;

    std::vector<KeyCallback> m_keyCallbacks;
    std::vector<MouseButtonCallback> m_mouseButtonCallbacks;
    std::vector<MouseMoveCallback> m_mouseMoveCallbacks;
};
}
