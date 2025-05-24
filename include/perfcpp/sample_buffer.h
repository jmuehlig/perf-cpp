#pragma once

#include "exception.h"
#include "feature.h"
#include "mmap_buffer.h"
#include "sample.h"
#include <cstdint>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace perf {
/**
 * The SampleBuffer manages the mmap-ed ringbuffer to store samples and handles overflows, i.e., the buffer is drained
 * and copied to a separate application-level buffer.
 */
class SampleBuffer
{
public:
  SampleBuffer(std::int32_t file_descriptor, std::uint64_t count_buffer_pages);

  SampleBuffer(SampleBuffer&& other) noexcept
    : _mmap_buffer(std::move(other._mmap_buffer))
    , _sample_buffers(std::move(other._sample_buffers))
    , _poll_and_handle_ringbuffer_overflow_thread(
        std::exchange(other._poll_and_handle_ringbuffer_overflow_thread, std::nullopt))
    , _cancel_thread_event_file_descriptor(std::exchange(other._cancel_thread_event_file_descriptor, std::nullopt))
  {
  }

  /**
   * The SampleBuffer should not be copied after initialization.
   */
  SampleBuffer(const SampleBuffer& other)
  {
    if (static_cast<bool>(other._mmap_buffer)) {
      throw CannotCopySampleBuffer{};
    }
  }

  ~SampleBuffer();

  /**
   * @return The entire sample data, including current buffer.
   */
  [[nodiscard]] std::vector<std::vector<std::byte>> consume_sample_data();

  /**
   * Reads the counter "live" without stopping via the "rdpmc" instruction.
   * Note that this is only possible on x86 architectures.
   *
   * @return The current value of the counter.
   */
  [[nodiscard]] std::uint64_t read_live() const noexcept { return _mmap_buffer.read_performance_monitoring_counter(); }

  /**
   * Polls on the given file descriptor in order to drain the mmap-ed buffer.
   *
   * @param perf_file_descriptor File descriptor of the mmap-ed buffer.
   * @param cancel_file_descriptor File descriptor for canceling the thread when closing the buffer.
   */
  void poll_and_handle_ringbuffer_overflow(std::int32_t perf_file_descriptor, std::int32_t cancel_file_descriptor);

private:
  MmapBuffer _mmap_buffer;

  /// Mutex for accessing the sample buffers and copying the current mmap-ed buffer. Both are accessed by the overflow
  /// handling buffer and the thread consuming the results.
  std::mutex _buffers_mutex;

  /// Separate buffer to copy data to when the mmap-ed buffer is near to full.
  std::vector<std::vector<std::byte>> _sample_buffers;

  /// Thread that is notified when the buffer is near to full and copies the data into a separated application-level
  /// buffer.
  std::optional<std::thread> _poll_and_handle_ringbuffer_overflow_thread;

  /// File descriptor used to cancel the ::select call the poll_and_handle thread is blocked by.
  std::optional<std::int32_t> _cancel_thread_event_file_descriptor{ std::nullopt };

  /**
   * Aligns the number of buffer pages to a number that is a power of two plus one for the header.
   *
   * @param number_of_buffer_pages Current number of buffer pages.
   * @return An aligned number that is a power of two plus one. Nothing changes if the number is already aligned.
   */
  [[nodiscard]] static std::uint64_t align_number_of_buffer_pages(std::uint64_t number_of_buffer_pages);
};
}