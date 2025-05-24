#include <iostream>
#include <perfcpp/hardware_info.h>
#include <perfcpp/sample_buffer.h>
#include <sys/eventfd.h>
#include <unistd.h>

perf::SampleBuffer::SampleBuffer(const std::int32_t file_descriptor, std::uint64_t count_buffer_pages)
{
  /// Align the number of buffer pages to hold a power of two + one for the header.
  count_buffer_pages = SampleBuffer::align_number_of_buffer_pages(count_buffer_pages);

  /// We only make use of buffer overflow handling (copy the data to an application-level buffer) when more than two
  /// pages are requested as two pages are allocated for live events which do not require draining the buffer.
  const auto is_need_overflow_handling = count_buffer_pages > 2ULL;

  /// Open the MMap Buffer.
  this->_mmap_buffer = MmapBuffer{ file_descriptor, is_need_overflow_handling, count_buffer_pages };

  /// Create a thread that copies data from the mmap-ed buffer into a application-level buffer whenever the buffer is
  /// near to overflow. This is only needed when overflowing is handled (which is not true for small buffers of two
  /// pages used for live counters).
  if (is_need_overflow_handling) {
    /// Create an event file descriptor to cancel the thread when closing the sample buffer.
    this->_cancel_thread_event_file_descriptor = ::eventfd(0, 0);
    if (this->_cancel_thread_event_file_descriptor.value() < 0) {
      throw CannotCreateEventFileDescriptor{ file_descriptor };
    }

    /// Allocate some space for sample buffers.
    this->_sample_buffers.reserve(32U);

    /// Create the handle thread.
    this->_poll_and_handle_ringbuffer_overflow_thread = std::thread(&SampleBuffer::poll_and_handle_ringbuffer_overflow,
                                                                    this,
                                                                    file_descriptor,
                                                                    this->_cancel_thread_event_file_descriptor.value());
  }
}

perf::SampleBuffer::~SampleBuffer()
{
  /// Notify the thread that copies data from the mmap-ed buffer into application-level buffers to cancel.
  if (const auto cancel_file_descriptor = std::exchange(this->_cancel_thread_event_file_descriptor, std::nullopt);
      cancel_file_descriptor.has_value()) {
    ::eventfd_write(cancel_file_descriptor.value(), 1);

    /// Wait for the thread to return.
    if (this->_poll_and_handle_ringbuffer_overflow_thread.has_value()) {
      this->_poll_and_handle_ringbuffer_overflow_thread->join();
    }

    /// Close the cancel file descriptor.
    ::close(cancel_file_descriptor.value());
  }
}

std::vector<std::vector<std::byte>>
perf::SampleBuffer::consume_sample_data()
{
  /// Lock the sample buffers vector. The thread polling for buffer overflows might copy data at the moment (or wants to
  /// do so while we are reading the data),
  auto lock_guard = std::lock_guard(this->_buffers_mutex);

  /// Move all the data out.
  auto sample_buffers = std::move(this->_sample_buffers);
  this->_sample_buffers = std::vector<std::vector<std::byte>>{};
  this->_sample_buffers.reserve(32U);

  /// Copy the current mmap buffer into the buffer that will be consumed.
  if (auto buffer = this->_mmap_buffer.copy_sample_data(); !buffer.empty()) {
    sample_buffers.push_back(std::move(buffer));
  }

  return sample_buffers;
}

void
perf::SampleBuffer::poll_and_handle_ringbuffer_overflow(const std::int32_t perf_file_descriptor,
                                                        const std::int32_t cancel_file_descriptor)
{
  do {
    /// Initialize the file descriptor set.
    auto file_descriptor_set = ::fd_set{};
    FD_ZERO(&file_descriptor_set);
    FD_SET(perf_file_descriptor, &file_descriptor_set);
    FD_SET(cancel_file_descriptor, &file_descriptor_set);

    const auto max_file_descriptor = std::max(perf_file_descriptor, cancel_file_descriptor) + 1;

    /// Block and wait for the perf file descriptor or the event file descriptor to notify.
    const auto ret = ::select(max_file_descriptor, &file_descriptor_set, nullptr, nullptr, nullptr);

    if (ret > 0) {
      /// If the cancel file descriptor is set, exit the loop and consequently the thread.
      if (FD_ISSET(cancel_file_descriptor, &file_descriptor_set)) {
        return;
      }

      /// If the "normal" perf file descriptor is set, drain the buffer by copying the data into an application-level
      /// buffer.
      if (FD_ISSET(perf_file_descriptor, &file_descriptor_set)) {
        /// Lock the sample buffers.
        auto lock_guard = std::lock_guard(this->_buffers_mutex);

        /// Copy the ringbuffer and append to sample buffers if any data available.
        if (auto buffer = this->_mmap_buffer.copy_sample_data(); !buffer.empty()) {
          this->_sample_buffers.push_back(std::move(buffer));
        }
      }
    } else if (ret == -1) {
      return;
    }
  } while (true);
}

std::uint64_t
perf::SampleBuffer::align_number_of_buffer_pages(std::uint64_t number_of_buffer_pages)
{
  /// Check if buffer pages is a power of two; if so, add one for the header as specified by the perf_event_open
  /// documentation.
  if ((number_of_buffer_pages & (number_of_buffer_pages - 1ULL)) == 0ULL) {
    /// Add one page for the header.
    return number_of_buffer_pages + 1ULL;
  }

  /// Check if buffer pages minus one is a power if to (i.e., the number includes already the additional page for the
  /// header). If not, we align the number to the next power of two and add one page for the header.
  if (((number_of_buffer_pages - 1ULL) & (number_of_buffer_pages - 2ULL)) != 0ULL) {
    --number_of_buffer_pages;
    number_of_buffer_pages |= number_of_buffer_pages >> 1;
    number_of_buffer_pages |= number_of_buffer_pages >> 2;
    number_of_buffer_pages |= number_of_buffer_pages >> 4;
    number_of_buffer_pages |= number_of_buffer_pages >> 8;
    number_of_buffer_pages |= number_of_buffer_pages >> 16;
    number_of_buffer_pages |= number_of_buffer_pages >> 32;

    /// Add one as we decremented, plus one for the page.
    return number_of_buffer_pages + 2ULL;
  }

  /// The number is already a power of two plus one; everything is correct configured.
  return number_of_buffer_pages;
}