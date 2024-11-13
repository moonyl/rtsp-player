#pragma once
#include <iostream>

inline int initializeGLFW() {
  if (!glfwInit()) {
    const char *error;
    glfwGetError(&error);
    throw std::runtime_error(std::string("GLFW 초기화 실패: ") + error);
  }

  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE); // GL_TRUE로 명시적으로 설정

  return 0; // 성공
}

// GLAD 초기화 함수
inline bool initGlad() {
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Glad 초기화 실패" << std::endl;
    return false;
  }
  return true;
}