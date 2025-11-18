// Utility: periodic heartbeat dots during long tests
#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

namespace testutil {

class Heartbeat {
 public:
  explicit Heartbeat(const char* label = nullptr,
                     std::chrono::milliseconds period = std::chrono::milliseconds(1000))
      : running_(true), period_(period) {
    if (label) { std::fprintf(stderr, "[hb] %s\n", label); std::fflush(stderr); }
    thr_ = std::thread([this]() {
      while (running_.load(std::memory_order_acquire)) {
        std::fputc('.', stderr);
        std::fflush(stderr);
        std::this_thread::sleep_for(period_);
      }
    });
  }
  ~Heartbeat() {
    running_.store(false, std::memory_order_release);
    if (thr_.joinable()) { thr_.join(); }
    std::fputc('\n', stderr);
    std::fflush(stderr);
  }

  Heartbeat(const Heartbeat&) = delete;
  Heartbeat& operator=(const Heartbeat&) = delete;

 private:
  std::atomic<bool> running_;
  std::chrono::milliseconds period_;
  std::thread thr_;
};

} // namespace testutil

