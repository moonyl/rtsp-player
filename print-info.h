#pragma once

// OpenGL 버전 및 렌더러 정보 출력 함수
inline void printOpenGLInfo() {
  const GLubyte *renderer = glGetString(GL_RENDERER);
  const GLubyte *version = glGetString(GL_VERSION);

  std::cout << "Renderer: " << renderer << std::endl;
  std::cout << "OpenGL Version: " << version << std::endl;
}