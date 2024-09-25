#pragma once

#include <cstdint>
#include <limits>

namespace perf {
/**
 * The precision controls the skid, which refers to the amount of
 * instructions between the event and the kernel records the sample.
 *
 * For more information see "precise_ip" on https://man7.org/linux/man-pages/man2/perf_event_open.2.html.
 */
enum Precision : std::uint8_t
{
  AllowArbitrarySkid = 0U,
  MustHaveConstantSkid = 1U,
  RequestZeroSkid = 2U,
  MustHaveZeroSkid = 3U,

  Unspecified = std::numeric_limits<std::uint8_t>::max()
};
}