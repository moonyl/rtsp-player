#pragma once

#include <queue>

// 프레임 버퍼 구조체
struct FrameBuffer {
  uint8_t *data;
  int size;
  int64_t pts;

  //   ~FrameBuffer() { av_free(data); } // 소멸자 추가하여 메모리 해제
};

// 프레임 큐 클래스 - 큐 관리를 캡슐화
class FrameQueue {
private:
  std::deque<FrameBuffer> frameQueue;
  std::mutex queueMutex;
  std::condition_variable queueCondition;
  size_t maxQueueSize;

public:
  FrameQueue(size_t maxSize) : maxQueueSize(maxSize) {}

  void push(const FrameBuffer &frame) {
    std::unique_lock<std::mutex> lock(queueMutex);
    queueCondition.wait(lock,
                        [this] { return frameQueue.size() < maxQueueSize; });
    frameQueue.push_back(frame);
    queueCondition.notify_one();
  }

  FrameBuffer pop() {
    std::unique_lock<std::mutex> lock(queueMutex);
    queueCondition.wait(lock, [this] { return !frameQueue.empty(); });
    FrameBuffer frame = frameQueue.front();
    frameQueue.pop_front();
    queueCondition.notify_one();
    return frame;
  }

  bool empty() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return frameQueue.empty();
  }

  size_t size() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return frameQueue.size();
  }
};
