#pragma once

#include <cstdint>
#include <utility>
#include <vector>
#include <cstddef>
#include <linux/perf_event.h>

namespace perf {
class MmapBuffer
{
public:
  MmapBuffer() noexcept = default;
  MmapBuffer(std::int32_t file_descriptor, bool is_write, std::uint64_t count_pages);
  ~MmapBuffer();

  MmapBuffer(MmapBuffer&& other) noexcept
    : _header(std::exchange(other._header, nullptr))
    , _count_pages(std::exchange(other._count_pages, 0ULL))
  {
  }

  MmapBuffer& operator=(MmapBuffer&& other) noexcept
  {
    _header = std::exchange(other._header, nullptr);
    _count_pages = std::exchange(other._count_pages, 0ULL);
    return *this;
  }

  /**
   * Reads a performance monitoring counter value from the mmap-ed buffer via the `rdpmc` instruction.
   *
   * @return PMC value read via `rdpmc` from the buffer.
   */
  [[nodiscard]] std::uint64_t read_performance_monitoring_counter() const noexcept;

  /**
   * Copies the sample data from the mmap-ed buffer into a vector and sets the tail of the mmap-ed buffer accordingly.
   *
   * @return Data copied from the buffer.
   */
  [[nodiscard]] std::vector<std::byte> copy_sample_data() noexcept;

  [[nodiscard]] explicit operator bool() const noexcept { return _header != nullptr; }

private:
  /// First page of the mmap-ed buffer; pointing to the buffer's header.
  perf_event_mmap_page* _header{ nullptr };

  /// Number of pages allocated via mmap.
  std::uint64_t _count_pages{ 0ULL };
};
}