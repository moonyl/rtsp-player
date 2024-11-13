#include "Renderer.h"
#include "FrameQueue.h"
#include "create-shader.h"
#include "create-window.h"
#include "debug-message-handler.h"
#include "init-gl.h"
#include "load-texture.h"
#include "print-info.h"
#include "render-scene.h"
#include "vertex-buffer.h"

const char *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord; // 텍스처 좌표 추가
out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord; // 텍스처 좌표 전송
}
)";

const char *fragmentShaderSource = R"(
#version 330 core
out vec4 fragColor;
in vec2 TexCoord;
uniform sampler2D texture1; // 텍스처 샘플러

void main() {
    fragColor = texture(texture1, vec2(TexCoord.x, 1.0 - TexCoord.y)); // 영상 상하 반전
}
)";

bool initializeOpenGL(GLFWwindow *window) {
  if (!initGlad())
    return false;
  GL_CALL(glEnable(GL_DEBUG_OUTPUT));
  GL_CALL(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
  GL_CALL(glDebugMessageCallback(MessageCallback, nullptr));
  GL_CALL(glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                                GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, GL_TRUE));
  printOpenGLInfo();
  return true;
}

Renderer::Renderer(int width, int height)
    : texture_(0), width_{width}, height_{height} {}

Renderer::~Renderer() {
  glDeleteProgram(shaderProgram_);
  glDeleteTextures(1, &texture_);
  glfwDestroyWindow(window_);
  glfwTerminate();
}

bool Renderer::initialize(const char *title) {
  if (initializeGLFW())
    return false;
  window_ = createGLFWWindow(title, 640, 360);
  if (!window_)
    return false;

  if (!initializeOpenGL(window_))
    return false;

  // auto vao_vbo_ebo = setupVAO();
  // if (!vao_vbo_ebo)
  //   return false;
  // buffers_.VAO = vao_vbo_ebo->VAO;
  // buffers_.VBO = vao_vbo_ebo->VBO;
  // buffers_.EBO = vao_vbo_ebo->EBO;
  buffers_ = setupVAO();

  shaderProgram_ =
      createShaderProgram(vertexShaderSource, fragmentShaderSource);
  if (shaderProgram_ == 0)
    return false;

  texture_ = loadTexture(width_, height_);
  if (texture_ == 0)
    return false;

  return true;
}

void Renderer::updateTexture(const FrameBuffer &decodedFrame) {
  ::updateTexture(texture_, decodedFrame, width_, height_);
}

void Renderer::render() {
  // renderScene 함수를 여기에 호출합니다.
  // renderScene(shaderProgram_, texture_, buffers_.VAO);
  renderScene(shaderProgram_, texture_, buffers_->VAO);
  glfwSwapBuffers(window_);
  glfwPollEvents();
}

bool Renderer::windowShouldClose() const {
  return glfwWindowShouldClose(window_);
}

std::unique_ptr<Buffers> initializeRendering(int width, int height) {
  initializeGLFW();
  GLFWwindow *window = createGLFWWindow("RTSP Player", 640, 360);
  if (!window)
    return nullptr;

  if (!initializeOpenGL(window))
    return nullptr;

  auto buffers = setupVAO();
  if (!buffers)
    return nullptr;

  unsigned int shaderProgram =
      createShaderProgram(vertexShaderSource, fragmentShaderSource);
  if (shaderProgram == 0)
    return nullptr;

  unsigned int texture = loadTexture(width, height);
  if (texture == 0)
    return nullptr;

  return buffers;
}