#pragma once

#include "exception.h"
#include "feature.h"
#include "mmap_buffer.h"
#include "sample.h"
#include "unique_file_descriptor.h"
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
  /**
   * Creates a sample buffer for a single page (only needed for live counters).
   *
   * @param file_descriptor File descriptor of the counter.
   */
  explicit SampleBuffer(const UniqueFileDescriptor& file_descriptor)
    : _mmap_buffer(file_descriptor, false, 1ULL)
  {
  }

  /**
   * Creates a sample buffer for multiple pages, including overflow handling.
   *
   * @param counter_file_descriptor File descriptor of the counter.
   * @param count_buffer_pages Number of buffer pages.
   */
  SampleBuffer(const UniqueFileDescriptor& counter_file_descriptor, std::uint64_t count_buffer_pages);

  SampleBuffer(SampleBuffer&& other) noexcept
    : _mmap_buffer(std::move(other._mmap_buffer))
    , _copied_sample_buffers(std::move(other._copied_sample_buffers))
    , _handle_buffer_overflow_thread(std::move(other._handle_buffer_overflow_thread))
    , _cancel_handle_overflow_thread_file_descriptor(std::move(other._cancel_handle_overflow_thread_file_descriptor))
  {
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
   * @param counter_file_descriptor File descriptor of the hardware counter.
   */
  void poll_and_handle_buffer_overflow(FileDescriptorView counter_file_descriptor);

private:
  MmapBuffer _mmap_buffer;

  /// Mutex for accessing the sample buffers and copying the current mmap-ed buffer. Both are accessed by the overflow
  /// handling buffer and the thread consuming the results.
  std::mutex _buffers_mutex;

  /// Separate buffers containing data copied from the mmap-ed buffer when it becomes full.
  std::vector<std::vector<std::byte>> _copied_sample_buffers;

  /// Thread that is notified when the buffer is near to full and copies the data into a separated application-level
  /// buffer.
  std::optional<std::thread> _handle_buffer_overflow_thread;

  /// File descriptor used to cancel the ::select call the poll_and_handle thread is blocked by.
  UniqueFileDescriptor _cancel_handle_overflow_thread_file_descriptor;

  /**
   * Aligns the number of buffer pages to a number that is a power of two plus one for the header.
   *
   * @param number_of_buffer_pages Current number of buffer pages.
   * @return An aligned number that is a power of two plus one. Nothing changes if the number is already aligned.
   */
  [[nodiscard]] static std::uint64_t align_number_of_buffer_pages(std::uint64_t number_of_buffer_pages);
};
}