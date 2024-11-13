#pragma once

// RTSP 스트림을 열고 비디오 스트림 정보를 가져오는 함수
inline std::pair<AVFormatContext *, int> open_rtsp_stream(const char *rtspUrl) {
  AVFormatContext *formatContext = nullptr;
  int videoStreamIndex = -1;

  AVDictionary *options = nullptr;
  av_dict_set(&options, "rtsp_transport", "tcp", 0);
  av_dict_set(&options, "stimeout", "5000000", 0);

  check_ffmpeg_error(
      avformat_open_input(&formatContext, rtspUrl, nullptr, &options),
      "Failed to open RTSP stream");
  check_ffmpeg_error(avformat_find_stream_info(formatContext, nullptr),
                     "Failed to find stream info");

  av_dict_free(&options); // options 메모리 해제

  for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
    if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStreamIndex = i;
      break;
    }
  }

  if (videoStreamIndex == -1) {
    avformat_close_input(&formatContext); // 자원 해제 추가
    throw std::runtime_error("Could not find video stream");
  }

  return std::make_pair(formatContext, videoStreamIndex);
}
