#pragma once

#include "FrameQueue.h"
// 전역 변수 - atomic bool 사용
std::atomic<bool> isPlaying{true};
FrameQueue frameQueue(30); // MAX_QUEUE_SIZE를 생성자 인자로 사용

class VideoTiming {
public:
  VideoTiming(AVFormatContext *formatContext, int videoStreamIndex)
      : fps_(av_q2d(formatContext->streams[videoStreamIndex]->r_frame_rate)),
        frame_delay_(1.0 / fps_),
        time_base_(formatContext->streams[videoStreamIndex]->time_base),
        time_base_double_(av_q2d(time_base_)) {
    if (fps_ <= 0) {
      throw std::runtime_error("Invalid FPS value.");
    }
    if (time_base_double_ <= 0) {
      throw std::runtime_error("Invalid time base value.");
    }
  }

  double getFPS() const { return fps_; }
  double getFrameDelay() const { return frame_delay_; }
  double getTimeBaseDouble() const { return time_base_double_; }
  AVRational getTimeBase() const { return time_base_; }

private:
  double fps_;
  double frame_delay_;
  AVRational time_base_;
  double time_base_double_;
};

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
  AVPixelFormat hw_transfer_format;
  bool using_hw_decoder;

  SwsContext *swsContext;

public:
  FFmpegDecoder()
      : _formatContext(nullptr), _codecContext(nullptr),
        //   swsContext(nullptr),
        hw_device_ctx(nullptr), _videoStreamIndex(-1), hw_frame(nullptr),
        sw_frame(nullptr), rgb_frame(nullptr),
        hw_transfer_format(AV_PIX_FMT_NONE), using_hw_decoder(false),
        swsContext(nullptr) {}

  bool initialize(
      AVFormatContext *formatContext, int videoStreamIndex /*AVCodecParameters
          *codecParams*/) {
    _formatContext = formatContext;
    _videoStreamIndex = videoStreamIndex;

    videoTiming =
        std::make_unique<VideoTiming>(formatContext, videoStreamIndex);
    // 코덱 설정
    AVCodecParameters *codecParams =
        formatContext->streams[videoStreamIndex]->codecpar;

    if (!initializeHWDecoder(codecParams)) {
      if (!initializeSWDecoder(codecParams)) {
        return false;
      }
    }

    return initializeCommon();
  }

  bool initializeHWDecoder(AVCodecParameters *codecParams) {
    // HW decoder 사용
    const AVCodec *codec = avcodec_find_decoder_by_name("h264_cuvid");
    if (!codec) {
      std::cerr << "Codec 'h264_cuvid' not found" << std::endl;
      return false;
    }

    _codecContext = avcodec_alloc_context3(codec);

    // HW device context 설정
    if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr,
                               nullptr, 0) < 0) {
      std::cerr << "Failed to create CUDA device context" << std::endl;
      avcodec_free_context(&_codecContext);
      return false;
    }

    _codecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    // 하드웨어 프레임 전송 포맷 확인
    hw_transfer_format = AV_PIX_FMT_NONE;
    for (int i = 0;; i++) {
      const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
      if (!config) {
        break;
      }
      if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
          config->device_type == AV_HWDEVICE_TYPE_CUDA) {
        hw_transfer_format = config->pix_fmt;
        break;
      }
    }

    if (hw_transfer_format == AV_PIX_FMT_NONE) {
      avcodec_free_context(&_codecContext);
      av_buffer_unref(&hw_device_ctx);
      return false;
    }

    // _codecContext->pkt_timebase =
    //     formatContext->streams[videoStreamIndex]->time_base;

    //   avcodec_parameters_to_context(codecContext, codecParams);
    //   avcodec_open2(codecContext, codec, nullptr);
    check_ffmpeg_error(
        avcodec_parameters_to_context(_codecContext, codecParams),
        "Failed to copy codec parameters to context");

    _codecContext->pkt_timebase =
        _formatContext->streams[_videoStreamIndex]->time_base;

    check_ffmpeg_error(avcodec_open2(_codecContext, codec, nullptr),
                       "Failed to open codec");

    // 모든 초기화가 성공한 경우에만 하드웨어 디코더 사용 플래그 설정
    using_hw_decoder = true;
    return true;
  }

  bool initializeSWDecoder(AVCodecParameters *codecParams) {
    const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
      return false;
    }

    _codecContext = avcodec_alloc_context3(codec);
    if (!_codecContext) {
      return false;
    }

    if (avcodec_parameters_to_context(_codecContext, codecParams) < 0) {
      avcodec_free_context(&_codecContext);
      return false;
    }

    _codecContext->pkt_timebase =
        _formatContext->streams[_videoStreamIndex]->time_base;

    if (avcodec_open2(_codecContext, codec, nullptr) < 0) {
      avcodec_free_context(&_codecContext);
      return false;
    }

    using_hw_decoder = false;
    return true;
  }
  bool initializeCommon() {

    // 프레임 및 패킷 할당
    hw_frame = av_frame_alloc();
    sw_frame = av_frame_alloc();
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

    sws_freeContext(swsContext);
  }

  AVFormatContext *formatContext() const { return _formatContext; }
  int videoStreamIndex() const { return _videoStreamIndex; }
  AVCodecContext *codecContext() const { return _codecContext; }

  using RetryCase = bool;
  std::optional<RetryCase> decode() {
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

          if (av_hwframe_transfer_data(frameCPU, hw_frame, 0) < 0) {
            std::cerr << "Failed to transfer frame to CPU" << std::endl;
            av_frame_free(&frameCPU);
            continue;
          }

          if (!swsContext) {
            AVPixelFormat pixFormat =
                static_cast<AVPixelFormat>(frameCPU->format);
            swsContext = sws_getContext(
                width(), height(), pixFormat /*codecContext->pix_fmt*/, width(),
                height(), AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr,
                nullptr);
          }
          // SwsContext 설정

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
void decodingThread(AVFormatContext *formatContext, int videoStreamIndex
                    /*FFmpegDecoder *decoder,*/ /*SwsContext *swsContext */) {
  FFmpegDecoder decoder;
  decoder.initialize(formatContext, videoStreamIndex);

  int width = decoder.width();
  int height = decoder.height();

  while (isPlaying) {
    auto result = decoder.decode();
    if (!result.has_value()) {
      break;
    }
  }
  decoder.deinitialize();
}
