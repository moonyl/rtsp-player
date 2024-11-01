#include <chrono>
#include <iostream> // iostream은 한 번만 포함합니다.
#include <thread>

#include <condition_variable>
#include <mutex>
#include <optional>
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

class FFmpegDecoder {
private:
  AVFormatContext *_formatContext;
  AVCodecContext *_codecContext;
  //   SwsContext *swsContext;
  AVBufferRef *hw_device_ctx;
  int _videoStreamIndex;

  AVFrame *hw_frame;
  AVFrame *sw_frame;
  AVFrame *rgb_frame;
  AVPacket *packet;
  uint8_t *buffer;
  int numBytes;

  std::unique_ptr<VideoTiming> videoTiming;

public:
  FFmpegDecoder()
      : _formatContext(nullptr), _codecContext(nullptr),
        //   swsContext(nullptr),
        hw_device_ctx(nullptr), _videoStreamIndex(-1), hw_frame(nullptr),
        sw_frame(nullptr), rgb_frame(nullptr) {}

  bool initialize(AVFormatContext *formatContext, int videoStreamIndex) {
    _formatContext = formatContext;
    _videoStreamIndex = videoStreamIndex;

    videoTiming =
        std::make_unique<VideoTiming>(formatContext, videoStreamIndex);
    // 코덱 설정
    AVCodecParameters *codecParams =
        formatContext->streams[videoStreamIndex]->codecpar;

    //   const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
    // HW decoder 사용
    const AVCodec *codec = avcodec_find_decoder_by_name("h264_cuvid");
    if (!codec) {
      std::cerr << "Codec 'h264_cuvid' not found" << std::endl;
      return false;
    }

    _codecContext = avcodec_alloc_context3(codec);

    // HW device context 설정
    AVBufferRef *hw_device_ctx = nullptr;
    if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr,
                               nullptr, 0) < 0) {
      std::cerr << "Failed to create CUDA device context" << std::endl;
      avcodec_free_context(&_codecContext);
      return false;
    }
    _codecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    _codecContext->pkt_timebase =
        formatContext->streams[videoStreamIndex]->time_base;

    //   avcodec_parameters_to_context(codecContext, codecParams);
    //   avcodec_open2(codecContext, codec, nullptr);
    check_ffmpeg_error(
        avcodec_parameters_to_context(_codecContext, codecParams),
        "Failed to copy codec parameters to context");
    check_ffmpeg_error(avcodec_open2(_codecContext, codec, nullptr),
                       "Failed to open codec");

    hw_frame = av_frame_alloc();
    rgb_frame = av_frame_alloc();
    packet = av_packet_alloc();

    if (!hw_frame || !rgb_frame || !packet) {
      std::cerr << "Memory allocation failed!" << std::endl;
      // 적절한 에러 처리 추가 (예: 함수 종료 또는 예외 발생)
      return false;
    }
    // RGB 버퍼 설정
    numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width(), height(), 1);
    buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    if (!buffer) {
      std::cerr << "Memory allocation failed!" << std::endl;
      av_frame_free(&hw_frame);
      av_frame_free(&rgb_frame);
      av_packet_free(&packet);
      return false;
    }

    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, buffer,
                         AV_PIX_FMT_RGB24, width(), height(), 1);
    return true;
  }

  int width() const { return _codecContext->width; }
  int height() const { return _codecContext->height; }

  void deinitialize() {
    avcodec_free_context(&_codecContext);
    av_frame_free(&hw_frame);
    av_frame_free(&rgb_frame);
    av_packet_free(&packet);
    av_free(buffer);
  }

  AVFormatContext *formatContext() const { return _formatContext; }
  int videoStreamIndex() const { return _videoStreamIndex; }
  AVCodecContext *codecContext() const { return _codecContext; }

  using RetryCase = bool;
  std::optional<RetryCase> decode(SwsContext *swsContext) {
    int ret = av_read_frame(_formatContext, packet);
    if (ret < 0) {
      // 에러 처리 개선:  av_read_frame의 에러 코드 확인
      if (ret != AVERROR_EOF) { // EOF는 예외처리
        std::cerr << "av_read_frame error: " << ret << std::endl;
        //  더 적절한 에러 처리 추가 (예: 재접속 시도 또는 함수 종료)
      }
      av_seek_frame(_formatContext, _videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
      return true; // 다음 반복으로 이동
    }

    if (packet->stream_index == _videoStreamIndex) {
      if (avcodec_send_packet(_codecContext, packet) == 0) {

        while (avcodec_receive_frame(_codecContext, hw_frame) == 0) {
          AVFrame *frameCPU = av_frame_alloc();
          if (!frameCPU) {
            std::cerr << "Failed to allocate frameCPU" << std::endl;
            continue; // 에러 처리
          }
          // // frameCPU의 크기 및 포맷 설정
          // frameCPU->width = codecContext->width;
          // frameCPU->height = codecContext->height;
          // frameCPU->format = AV_PIX_FMT_RGB24;

          // // 메모리 할당
          // if (av_frame_get_buffer(frameCPU, 0) < 0) {
          //   std::cerr << "Failed to allocate buffer for frameCPU" <<
          //   std::endl; av_frame_free(&frameCPU); continue;
          // }

          if (av_hwframe_transfer_data(frameCPU, hw_frame, 0) < 0) {
            std::cerr << "Failed to transfer frame to CPU" << std::endl;
            av_frame_free(&frameCPU);
            continue;
          }

          if (!frameCPU || !frameCPU->data[0]) {
            std::cerr << "frameCPU is invalid" << std::endl;
            continue; // frameCPU가 유효하지 않음
          }

          if (!rgb_frame || !rgb_frame->data[0]) {
            std::cerr << "frameRGB is invalid" << std::endl;
            continue; // frameRGB가 유효하지 않음
          }

          sws_scale(swsContext, frameCPU->data, frameCPU->linesize, 0, height(),
                    rgb_frame->data, rgb_frame->linesize);
          av_frame_free(&frameCPU);

          // sws_scale(swsContext, frame->data, frame->linesize, 0, height,
          //           frameRGB->data, frameRGB->linesize);

          FrameBuffer fb;
          fb.size = numBytes;
          fb.data = (uint8_t *)av_malloc(numBytes);
          if (!fb.data) {
            std::cerr << "Memory allocation failed!" << std::endl;
            return std::nullopt; // 메모리 할당 실패시 루프 종료
          }
          memcpy(fb.data, rgb_frame->data[0], numBytes);
          fb.pts = static_cast<int64_t>(
              hw_frame->pts *
              videoTiming
                  ->getTimeBaseDouble()); // time_base를 고려하여 pts 보정

          frameQueue.push(fb);
          break;
        }
      }
    }
    av_packet_unref(packet);
    return false;
  }
};

// 디코딩 스레드 함수
void decodingThread(FFmpegDecoder *decoder, SwsContext *swsContext) {
  //   auto formatContext = decoder->formatContext();
  //   int videoStreamIndex = decoder->videoStreamIndex();
  //   int width = decoder->width();
  //   int height = decoder->height();
  //   auto codecContext = decoder->codecContext();
  //   VideoTiming videoTiming(formatContext, videoStreamIndex);

  //   AVFrame *frame = av_frame_alloc();
  //   AVFrame *frameRGB = av_frame_alloc();
  //   AVPacket *packet = av_packet_alloc();

  //   if (!frame || !frameRGB || !packet) {
  //     std::cerr << "Memory allocation failed!" << std::endl;
  //     // 적절한 에러 처리 추가 (예: 함수 종료 또는 예외 발생)
  //     return;
  //   }

  //   // RGB 버퍼 설정
  //   int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height,
  //   1); uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
  //   if (!buffer) {
  //     std::cerr << "Memory allocation failed!" << std::endl;
  //     av_frame_free(&frame);
  //     av_frame_free(&frameRGB);
  //     av_packet_free(&packet);
  //     return;
  //   }

  //   av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer,
  //                        AV_PIX_FMT_RGB24, width, height, 1);

  while (isPlaying) {
    auto result = decoder->decode(swsContext);
    if (!result.has_value()) {
      break;
    }
    // int ret = av_read_frame(formatContext, packet);
    // if (ret < 0) {
    //   // 에러 처리 개선:  av_read_frame의 에러 코드 확인
    //   if (ret != AVERROR_EOF) { // EOF는 예외처리
    //     std::cerr << "av_read_frame error: " << ret << std::endl;
    //     //  더 적절한 에러 처리 추가 (예: 재접속 시도 또는 함수 종료)
    //   }
    //   av_seek_frame(formatContext, videoStreamIndex, 0,
    //   AVSEEK_FLAG_BACKWARD); continue; // 다음 반복으로 이동
    // }

    // if (packet->stream_index == videoStreamIndex) {
    //   if (avcodec_send_packet(codecContext, packet) == 0) {

    //     while (avcodec_receive_frame(codecContext, frame) == 0) {
    //       AVFrame *frameCPU = av_frame_alloc();
    //       if (!frameCPU) {
    //         std::cerr << "Failed to allocate frameCPU" << std::endl;
    //         continue; // 에러 처리
    //       }
    //       // // frameCPU의 크기 및 포맷 설정
    //       // frameCPU->width = codecContext->width;
    //       // frameCPU->height = codecContext->height;
    //       // frameCPU->format = AV_PIX_FMT_RGB24;

    //       // // 메모리 할당
    //       // if (av_frame_get_buffer(frameCPU, 0) < 0) {
    //       //   std::cerr << "Failed to allocate buffer for frameCPU" <<
    //       //   std::endl; av_frame_free(&frameCPU); continue;
    //       // }

    //       if (av_hwframe_transfer_data(frameCPU, frame, 0) < 0) {
    //         std::cerr << "Failed to transfer frame to CPU" << std::endl;
    //         av_frame_free(&frameCPU);
    //         continue;
    //       }

    //       if (!frameCPU || !frameCPU->data[0]) {
    //         std::cerr << "frameCPU is invalid" << std::endl;
    //         continue; // frameCPU가 유효하지 않음
    //       }

    //       if (!frameRGB || !frameRGB->data[0]) {
    //         std::cerr << "frameRGB is invalid" << std::endl;
    //         continue; // frameRGB가 유효하지 않음
    //       }

    //       sws_scale(swsContext, frameCPU->data, frameCPU->linesize, 0,
    //       height,
    //                 frameRGB->data, frameRGB->linesize);
    //       av_frame_free(&frameCPU);

    //       // sws_scale(swsContext, frame->data, frame->linesize, 0, height,
    //       //           frameRGB->data, frameRGB->linesize);

    //       FrameBuffer fb;
    //       fb.size = numBytes;
    //       fb.data = (uint8_t *)av_malloc(numBytes);
    //       if (!fb.data) {
    //         std::cerr << "Memory allocation failed!" << std::endl;
    //         break; // 메모리 할당 실패시 루프 종료
    //       }
    //       memcpy(fb.data, frameRGB->data[0], numBytes);
    //       fb.pts = static_cast<int64_t>(
    //           frame->pts *
    //           videoTiming.getTimeBaseDouble()); // time_base를 고려하여 pts
    //           보정

    //       frameQueue.push(fb);
    //     }
    //   }
    // }
    // av_packet_unref(packet);
  }

  //   av_frame_free(&frame);
  //   av_frame_free(&frameRGB);
  //   av_packet_free(&packet);
  //   av_free(buffer);
}

int main() {
  const char *rtspUrl =
      "rtsp://admin:q1w2e3r4@@192.168.15.83:50554/rtsp/camera1/high";
  // FFmpeg 네트워king 초기화
  avformat_network_init();

  auto [formatContext, videoStreamIndex] = open_rtsp_stream(rtspUrl);

  FFmpegDecoder decoder;
  decoder.initialize(formatContext, videoStreamIndex);
  //   // 코덱 설정
  //   AVCodecParameters *codecParams =
  //       formatContext->streams[videoStreamIndex]->codecpar;

  //   //   const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
  //   // HW decoder 사용
  //   const AVCodec *codec = avcodec_find_decoder_by_name("h264_cuvid");
  //   if (!codec) {
  //     std::cerr << "Codec 'h264_cuvid' not found" << std::endl;
  //     return -1;
  //   }

  //   AVCodecContext *codecContext = avcodec_alloc_context3(codec);

  //   // HW device context 설정
  //   AVBufferRef *hw_device_ctx = nullptr;
  //   if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA,
  //   nullptr,
  //                              nullptr, 0) < 0) {
  //     std::cerr << "Failed to create CUDA device context" << std::endl;
  //     avcodec_free_context(&codecContext);
  //     return -1;
  //   }
  //   codecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);
  //   codecContext->pkt_timebase =
  //       formatContext->streams[videoStreamIndex]->time_base;

  //   //   avcodec_parameters_to_context(codecContext, codecParams);
  //   //   avcodec_open2(codecContext, codec, nullptr);
  //   check_ffmpeg_error(avcodec_parameters_to_context(codecContext,
  //   codecParams),
  //                      "Failed to copy codec parameters to context");
  //   check_ffmpeg_error(avcodec_open2(codecContext, codec, nullptr),
  //                      "Failed to open codec");

  int width = decoder.width();
  int height = decoder.height();
  // SwsContext 설정
  SwsContext *swsContext = sws_getContext(
      width, height, AV_PIX_FMT_NV12 /*codecContext->pix_fmt*/, width, height,
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

  unsigned int texture = loadTexture(width, height);
  if (texture == 0) {
    std::cerr << "텍스처 로드 실패" << std::endl;
    return -1;
  }

  // 디코딩 스레드 시작
  std::thread decodeThread(decodingThread, &decoder, swsContext);
  // 메인 루프
  while (!glfwWindowShouldClose(window)) {
    FrameBuffer decodedFrame;
    if (!frameQueue.empty()) {
      decodedFrame = frameQueue.pop();
      updateTexture(texture, decodedFrame, width, height);
    }

    renderScene(shaderProgram, texture, buffers->VAO); // buffers->VAO 사용

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // 정리
  isPlaying = false;
  decodeThread.join();

  decoder.deinitialize();
  //   avcodec_free_context(&codecContext);
  avformat_close_input(&formatContext);
  sws_freeContext(swsContext);

  glDeleteProgram(shaderProgram);

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}