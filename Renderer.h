#pragma once

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <memory>
struct FrameBuffer;
struct Buffers;

class Renderer {
public:
  Renderer(int width, int height);
  ~Renderer();

  bool initialize(const char *title);
  void render();
  bool isValid() const { return window_ != nullptr; }
  bool windowShouldClose() const;
  void updateTexture(const FrameBuffer &decodedFrame);

private:
  int width_;
  int height_;
  GLFWwindow *window_;
  GLuint texture_;
  unsigned int shaderProgram_;
  // Buffers buffers_; // OpenGL 버퍼들만 포함
  std::unique_ptr<Buffers> buffers_;
};