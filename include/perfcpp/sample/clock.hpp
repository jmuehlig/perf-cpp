#pragma once

#include <cstdint>

namespace perf {
/**
 * Clock source used for timestamps in perf sample records (PERF_SAMPLE_TIME).
 *
 * Maps to POSIX clockid_t values accepted by perf_event_open(2) when use_clockid is set.
 * Using a named clock lets perf timestamps be compared directly with clock_gettime() values
 * from user-space without an offset calibration step.
 */
enum class Clock : std::int32_t
{
  /// Wall-clock time (UTC); can jump on NTP adjustments.
  Realtime = 0,

  /// Monotonic clock; does not count time spent suspended.
  Monotonic = 1,

  /// Monotonic clock not subject to NTP frequency adjustments.
  MonotonicRaw = 4,

  /// Low-resolution (coarse) version of Realtime; cheaper to read.
  RealtimeCoarse = 5,

  /// Low-resolution (coarse) version of Monotonic; cheaper to read.
  MonotonicCoarse = 6,

  /// Monotonic clock that includes time the system was suspended.
  Boottime = 7,

  /// International Atomic Time; monotonic, no leap-second smearing.
  Tai = 11,
};
}
