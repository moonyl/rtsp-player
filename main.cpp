#include <chrono>
#include <iostream> // iostream은 한 번만 포함합니다.
#include <optional>
#include <string>
#include <thread>

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

#include "Renderer.h"
#include "check-error.h"
#include "create-shader.h"
#include "create-window.h"
#include "debug-message-handler.h"
#include "decode-thread.h"
#include "init-gl.h"
#include "load-texture.h"
#include "open-rtsp.h"
#include "print-info.h"
#include "render-scene.h"
#include "vertex-buffer.h"

std::pair<AVFormatContext *, int> openStreamAndInitialize(const char *rtspUrl) {
  avformat_network_init();
  return open_rtsp_stream(rtspUrl);
}

int main() {
  auto [formatContext, videoStreamIndex] =
      openStreamAndInitialize("rtsp://192.168.15.27/towncenter.mkv");

  // 코덱 설정
  AVCodecParameters *codecParams =
      formatContext->streams[videoStreamIndex]->codecpar;
  auto width = codecParams->width;
  auto height = codecParams->height;

  // initializeGLFW();
  // GLFWwindow *window = createGLFWWindow("RTSP Player", 640, 360);

  // if (!initializeOpenGL(window))
  //   return -1;

  // auto buffers = setupVAO();
  // if (!buffers) {
  //   std::cerr << "VAO/VBO/EBO 생성 실패" << std::endl;
  //   return -1;
  // }

  // unsigned int shaderProgram =
  //     createShaderProgram(vertexShaderSource, fragmentShaderSource);
  // if (shaderProgram == 0) {
  //   std::cerr << "셰이더 프로그램 생성 실패" << std::endl;
  //   return -1;
  // }

  // unsigned int texture = loadTexture(width, height);
  // if (texture == 0) {
  //   std::cerr << "텍스처 로드 실패" << std::endl;
  //   return -1;
  // }

  // auto buffers = initializeRendering(width, height);

  Renderer renderer(width, height);
  if (!renderer.initialize("RTSP Player")) {
    std::cerr << "Renderer 초기화 실패" << std::endl;
    return -1;
  }

  // 디코딩 스레드 시작
  std::thread decodeThread(decodingThread,
                           /*&decoder, swsContext*/ formatContext,
                           videoStreamIndex);
  // 메인 루프
  while (!renderer.windowShouldClose()) {
    FrameBuffer decodedFrame;
    if (!frameQueue.empty()) {
      decodedFrame = frameQueue.pop();
      // updateTexture(renderer.texture_, decodedFrame, width, height);
      renderer.updateTexture(decodedFrame);
    }

    renderer.render();

    // renderScene(shaderProgram, texture, buffers->VAO); // buffers->VAO 사용
    // glfwSwapBuffers(window);
    // glfwPollEvents();
  }

  // 정리
  isPlaying = false;
  decodeThread.join();

  //   avcodec_free_context(&codecContext);
  avformat_close_input(&formatContext);

  // glDeleteProgram(shaderProgram);

  // glfwDestroyWindow(window);
  // glfwTerminate();

  return 0;
}