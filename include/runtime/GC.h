/***
 * Name: GC controls API
 * Purpose: Configure and drive the garbage collector; access stats and telemetry.
 */
#pragma once

#include <cstddef>
#include "runtime/GCStats.h"

namespace pycc::rt {
    void gc_set_threshold(std::size_t bytes);

    void gc_set_conservative(bool enabled);

    void gc_set_background(bool enabled);

    void gc_collect();

    void gc_register_root(void **addr);

    void gc_unregister_root(void **addr);

    RuntimeStats gc_stats();

    GcTelemetry gc_telemetry();

    // Barrier mode (0 = incremental-update, 1 = SATB)
    void gc_set_barrier_mode(int mode);

    void gc_pre_barrier(void **slot);

    void gc_reset_for_tests();
} // namespace pycc::rt
