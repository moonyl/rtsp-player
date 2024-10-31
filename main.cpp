#include <chrono>
#include <iostream> // iostream은 한 번만 포함합니다.
#include <thread>

#include <condition_variable>
#include <mutex>
#include <string>

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

int main() {
  const char *rtspUrl =
      "rtsp://admin:q1w2e3r4@@192.168.15.83:50554/rtsp/camera1/high";
  // FFmpeg 네트워king 초기화
  avformat_network_init();

  auto [formatContext, videoStreamIndex] = open_rtsp_stream(rtspUrl);

  // 코덱 설정
  AVCodecParameters *codecParams =
      formatContext->streams[videoStreamIndex]->codecpar;

  //   const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
  // HW decoder 사용
  const AVCodec *codec = avcodec_find_decoder_by_name("h264_cuvid");
  if (!codec) {
    std::cerr << "Codec 'h264_cuvid' not found" << std::endl;
    return -1;
  }

  AVCodecContext *codecContext = avcodec_alloc_context3(codec);

  // HW device context 설정
  AVBufferRef *hw_device_ctx = nullptr;
  if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr,
                             nullptr, 0) < 0) {
    std::cerr << "Failed to create CUDA device context" << std::endl;
    avcodec_free_context(&codecContext);
    return -1;
  }
  codecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);
  codecContext->pkt_timebase =
      formatContext->streams[videoStreamIndex]->time_base;

  //   avcodec_parameters_to_context(codecContext, codecParams);
  //   avcodec_open2(codecContext, codec, nullptr);
  check_ffmpeg_error(avcodec_parameters_to_context(codecContext, codecParams),
                     "Failed to copy codec parameters to context");
  check_ffmpeg_error(avcodec_open2(codecContext, codec, nullptr),
                     "Failed to open codec");

  // SwsContext 설정
  SwsContext *swsContext =
      sws_getContext(codecContext->width, codecContext->height,
                     AV_PIX_FMT_NV12 /*codecContext->pix_fmt*/,
                     codecContext->width, codecContext->height,
                     AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);

  initializeGLFW();
  GLFWwindow *window = createGLFWWindow("RTSP Player", 640, 360);

  if (!initGlad())
    return -1;

  GL_CALL(glEnable(GL_DEBUG_OUTPUT));
  GL_CALL(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
  GL_CALL(glDebugMessageCallback(MessageCallback, nullptr));
  GL_CALL(glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                                GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, GL_TRUE));

  printOpenGLInfo();

  auto buffers = setupVAO();
  if (!buffers) {
    std::cerr << "VAO/VBO/EBO 생성 실패" << std::endl;
    return -1;
  }

  unsigned int shaderProgram =
      createShaderProgram(vertexShaderSource, fragmentShaderSource);
  if (shaderProgram == 0) {
    std::cerr << "셰이더 프로그램 생성 실패" << std::endl;
    return -1;
  }

  unsigned int texture = loadTexture(codecContext->width, codecContext->height);
  if (texture == 0) {
    std::cerr << "텍스처 로드 실패" << std::endl;
    return -1;
  }

  // 디코딩 스레드 시작
  std::thread decoder(decodingThread, formatContext, videoStreamIndex,
                      codecContext, swsContext, codecContext->width,
                      codecContext->height);
  // 메인 루프
  while (!glfwWindowShouldClose(window)) {
    FrameBuffer decodedFrame;
    if (!frameQueue.empty()) {
      decodedFrame = frameQueue.pop();
      updateTexture(texture, decodedFrame, codecContext);
    }

    renderScene(shaderProgram, texture, buffers->VAO); // buffers->VAO 사용

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // 정리
  isPlaying = false;
  decoder.join();

  avcodec_free_context(&codecContext);
  avformat_close_input(&formatContext);
  sws_freeContext(swsContext);

  glDeleteProgram(shaderProgram);

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}