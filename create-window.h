#pragma once

GLFWwindow *createGLFWWindow(const char *title, int width, int height) {
  GLFWwindow *window = glfwCreateWindow(width, height, title, NULL, NULL);
  if (!window) {
    const char *error;
    glfwGetError(&error);
    throw std::runtime_error(std::string("GLFW 윈도우 생성 실패: ") + error);
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(0); // VSync 비활성화

  return window;
}