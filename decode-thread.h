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

// 디코딩 스레드 함수
void decodingThread(AVFormatContext *formatContext, int videoStreamIndex,
                    AVCodecContext *codecContext, SwsContext *swsContext,
                    int width, int height) {
  VideoTiming videoTiming(formatContext, videoStreamIndex);

  AVFrame *frame = av_frame_alloc();
  AVFrame *frameRGB = av_frame_alloc();
  AVPacket *packet = av_packet_alloc();

  if (!frame || !frameRGB || !packet) {
    std::cerr << "Memory allocation failed!" << std::endl;
    // 적절한 에러 처리 추가 (예: 함수 종료 또는 예외 발생)
    return;
  }

  // RGB 버퍼 설정
  int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
  uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
  if (!buffer) {
    std::cerr << "Memory allocation failed!" << std::endl;
    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    av_packet_free(&packet);
    return;
  }

  av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer,
                       AV_PIX_FMT_RGB24, width, height, 1);

  while (isPlaying) {
    int ret = av_read_frame(formatContext, packet);
    if (ret < 0) {
      // 에러 처리 개선:  av_read_frame의 에러 코드 확인
      if (ret != AVERROR_EOF) { // EOF는 예외처리
        std::cerr << "av_read_frame error: " << ret << std::endl;
        //  더 적절한 에러 처리 추가 (예: 재접속 시도 또는 함수 종료)
      }
      av_seek_frame(formatContext, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
      continue; // 다음 반복으로 이동
    }

    if (packet->stream_index == videoStreamIndex) {
      if (avcodec_send_packet(codecContext, packet) == 0) {
        while (avcodec_receive_frame(codecContext, frame) == 0) {
          sws_scale(swsContext, frame->data, frame->linesize, 0, height,
                    frameRGB->data, frameRGB->linesize);

          FrameBuffer fb;
          fb.size = numBytes;
          fb.data = (uint8_t *)av_malloc(numBytes);
          if (!fb.data) {
            std::cerr << "Memory allocation failed!" << std::endl;
            break; // 메모리 할당 실패시 루프 종료
          }
          memcpy(fb.data, frameRGB->data[0], numBytes);
          fb.pts = static_cast<int64_t>(
              frame->pts *
              videoTiming.getTimeBaseDouble()); // time_base를 고려하여 pts 보정

          frameQueue.push(fb);
        }
      }
    }
    av_packet_unref(packet);
  }

  av_frame_free(&frame);
  av_frame_free(&frameRGB);
  av_packet_free(&packet);
  av_free(buffer);
}