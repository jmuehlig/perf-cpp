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

public:
  /**
   * The UserLevelBufferEntry represents an entry in the user-level buffer filled by the perf subsystem by parsing the
   * hardware-related samples. This helper assists in consuming data from the buffer and turning it into Samples.
   */
  class Entry
  {
  public:
    explicit Entry(const std::uintptr_t iterator) noexcept
      : _header(reinterpret_cast<perf_event_header*>(iterator))
      , _data(std::uintptr_t(_header + 1U))
    {
    }

    Entry(Entry&& other) noexcept
      : _header(std::exchange(other._header, nullptr))
      , _data(std::exchange(other._data, 0ULL))
    {
    }

    ~Entry() noexcept = default;

    [[nodiscard]] std::optional<Metadata::Mode> mode() const noexcept;
    [[nodiscard]] std::uint16_t size() const noexcept { return _header->size; }

    template<typename T>
    [[nodiscard]] T read() noexcept
    {
      const auto data = *reinterpret_cast<T*>(_data);
      _data += sizeof(T);

      return data;
    }

    template<typename T>
    [[nodiscard]] const T* read(const std::size_t size) noexcept
    {
      auto* begin = reinterpret_cast<T*>(_data);
      _data += sizeof(T) * size;

      return begin;
    }

    template<typename T>
    void skip() noexcept
    {
      _data += sizeof(T);
    }

    template<typename T>
    void skip(const std::size_t size) noexcept
    {
      _data += sizeof(T) * size;
    }

    template<typename T>
    T as() const noexcept
    {
      return reinterpret_cast<T>(_data);
    }

    [[nodiscard]] bool is_sample_event() const noexcept { return _header->type == PERF_RECORD_SAMPLE; }
    [[nodiscard]] bool is_loss_event() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_LOST_SAMPLES /// PERF_RECORD_LOST_SAMPLES is supported since Linux 4.2
      return _header->type == PERF_RECORD_LOST_SAMPLES;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_context_switch_event() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_SWITCH /// Switch events are supported since Linux 4.3
      return _header->type == PERF_RECORD_SWITCH || _header->type == PERF_RECORD_SWITCH_CPU_WIDE;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_context_switch_cpu_wide() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_SWITCH /// Switch events are supported since Linux 4.3
      return _header->type == PERF_RECORD_SWITCH_CPU_WIDE;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_cgroup_event() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_CGROUP /// cgroup events is supported since Linux 5.7
      return _header->type == PERF_RECORD_CGROUP;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_throttle_event() const noexcept
    {
      return _header->type == PERF_RECORD_THROTTLE || _header->type == PERF_RECORD_UNTHROTTLE;
    }
    [[nodiscard]] bool is_throttle() const noexcept { return _header->type == PERF_RECORD_THROTTLE; }

    [[nodiscard]] bool is_exact_ip() const noexcept { return _header->misc & PERF_RECORD_MISC_EXACT_IP; }
    [[nodiscard]] bool is_context_switch_out() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_SWITCH /// Switch events are supported since Linux 4.3
      return _header->misc & PERF_RECORD_MISC_SWITCH_OUT;
#else
      return false;
#endif
    }
    [[nodiscard]] bool is_context_switch_out_preempt() const noexcept
    {
#ifndef PERFCPP_NO_RECORD_MISC_SWITCH_OUT_PREEMPT /// Preempt flag of switch events is supported since Linux 4.3
      return _header->misc & PERF_RECORD_MISC_SWITCH_OUT_PREEMPT;
#else
      return false;
#endif
    }

  private:
    /// Header of the event.
    perf_event_header* _header;

    /// Data (located directly after the header).
    std::uintptr_t _data;
  };
};
}