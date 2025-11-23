/***
 * Name: pycc::rt::RuntimeStats, GcTelemetry
 * Purpose: Expose GC counters and telemetry to tests and tooling.
 */
#pragma once

#include <cstdint>

namespace pycc::rt {
    struct RuntimeStats {
        uint64_t numAllocated{0};
        uint64_t numFreed{0};
        uint64_t numCollections{0};
        uint64_t bytesAllocated{0};
        uint64_t bytesLive{0};
        uint64_t peakBytesLive{0};
        uint64_t lastReclaimedBytes{0};
    };

    struct GcTelemetry {
        double allocRateBytesPerSec{0.0}; // recent bytes/sec
        double pressure{0.0}; // bytesLive / threshold (0..inf)
    };
} // namespace pycc::rt
