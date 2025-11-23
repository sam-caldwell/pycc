/**
 * Simple runtime GC benchmark: compares throughput with background GC on vs. off.
 * Usage: bench_gc [iters] [size]
 */
#include "runtime/All.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace pycc::rt;

int main(int argc, char** argv) {
  std::size_t iters = (argc > 1) ? static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10)) : 200000;
  std::size_t size  = (argc > 2) ? static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10)) : 24;

  auto run = [&](bool bg, int barrierMode) {
    gc_reset_for_tests();
    gc_set_threshold(1 << 20); // 1MiB
    gc_set_conservative(false);
    gc_set_background(bg);
    gc_set_barrier_mode(barrierMode);

    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iters; ++i) {
      // allocate strings and boxed values
      std::string s(size, 'x');
      (void)string_new(s.c_str(), s.size());
      (void)box_int(static_cast<int64_t>(i));
      (void)box_float(static_cast<double>(i) * 0.5);
      (void)box_bool((i & 1U) != 0U);
      if ((i % 10000U) == 0U) { /* yield */ }
    }
    const auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    auto st = gc_stats();
    auto tel = gc_telemetry();
    std::cout << (bg ? "[bg=on]" : "[bg=off]")
              << (barrierMode == 1 ? "[satb]" : "[inc]")
              << " iters=" << iters
              << " size=" << size
              << " time_ms=" << ms
              << " collections=" << st.numCollections
              << " bytes_alloc=" << st.bytesAllocated
              << " bytes_live=" << st.bytesLive
              << " peak_live=" << st.peakBytesLive
              << " last_reclaimed=" << st.lastReclaimedBytes
              << " alloc_rate_bps=" << static_cast<long long>(tel.allocRateBytesPerSec)
              << " pressure=" << tel.pressure
              << "\n";
  };

  run(false, 0);
  run(true, 0);
  run(true, 1);
  return 0;
}
