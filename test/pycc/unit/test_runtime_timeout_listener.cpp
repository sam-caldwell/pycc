// GTest per-test timeout listener (120s default) for runtime-only binary
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {
class TimeoutListener : public ::testing::EmptyTestEventListener {
 public:
  explicit TimeoutListener(int seconds) : limitSeconds_(seconds) {}
  void OnTestStart(const ::testing::TestInfo& ti) override {
    cancel_.store(false, std::memory_order_relaxed);
    worker_ = std::thread([this, name = std::string(ti.test_suite_name()) + "." + ti.name()]() {
      using namespace std::chrono_literals;
      auto limit = std::chrono::seconds(limitSeconds_);
      auto start = std::chrono::steady_clock::now();
      while (!cancel_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(100ms);
        if (std::chrono::steady_clock::now() - start > limit) {
          std::cerr << "\n[timeout] Test exceeded " << limitSeconds_ << "s: " << name << "\n" << std::flush;
          std::abort();
        }
      }
    });
  }
  void OnTestEnd(const ::testing::TestInfo&) override {
    cancel_.store(true, std::memory_order_relaxed);
    if (worker_.joinable()) worker_.join();
  }
 private:
  int limitSeconds_;
  std::atomic<bool> cancel_{false};
  std::thread worker_;
};

class TimeoutEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    int secs = 120;
    if (const char* env = std::getenv("PYCC_GTEST_TIMEOUT_SECS")) { int v = std::atoi(env); if (v > 0) secs = v; }
    ::testing::UnitTest::GetInstance()->listeners().Append(new TimeoutListener(secs));
  }
};

auto* const kTimeoutEnvReg2 = ::testing::AddGlobalTestEnvironment(new TimeoutEnv());
} // namespace
