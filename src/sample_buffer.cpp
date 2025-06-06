#include <iostream>
#include <perfcpp/hardware_info.h>
#include <perfcpp/sample_buffer.h>
#include <sys/eventfd.h>
#include <sys/select.h>

perf::SampleBuffer::SampleBuffer(const UniqueFileDescriptor& counter_file_descriptor, std::uint64_t count_buffer_pages)
{
  /// Align the number of buffer pages to hold a power of two + one for the header.
  count_buffer_pages = SampleBuffer::align_number_of_buffer_pages(count_buffer_pages);

  /// Open the MMap Buffer.
  this->_mmap_buffer = MmapBuffer{ counter_file_descriptor, true, count_buffer_pages };

  /// Create an event file descriptor to cancel the thread when closing the sample buffer.
  this->_cancel_handle_overflow_thread_file_descriptor = UniqueFileDescriptor{ ::eventfd(0, 0) };
  if (!this->_cancel_handle_overflow_thread_file_descriptor.has_value()) {
    throw CannotCreateEventFileDescriptor{};
  }

  /// Allocate some space for sample buffers.
  this->_copied_sample_buffers.reserve(32U);

  /// Create the thread that handles overflows in the mmap buffer and copies samples into a application-level buffer.
  this->_handle_buffer_overflow_thread =
    std::thread(&SampleBuffer::poll_and_handle_buffer_overflow, this, FileDescriptorView{ counter_file_descriptor });
}

perf::SampleBuffer::~SampleBuffer()
{
  /// Notify the thread that copies data from the mmap-ed buffer into application-level buffers to cancel.
  if (this->_cancel_handle_overflow_thread_file_descriptor.has_value()) {
    ::eventfd_write(this->_cancel_handle_overflow_thread_file_descriptor.value(), 1);
  }

  /// Wait for the thread to return.
  if (this->_handle_buffer_overflow_thread.has_value()) {
    this->_handle_buffer_overflow_thread->join();
  }
}

std::vector<std::vector<std::byte>>
perf::SampleBuffer::consume_sample_data()
{
  /// Lock the sample buffers vector. The thread polling for buffer overflows might copy data at the moment (or wants to
  /// do so while we are reading the data),
  auto lock_guard = std::lock_guard(this->_buffers_mutex);

  /// Move all the data out of the sample buffer into a buffer that will be passed to the caller of this function and
  /// create a new one that will be used for further sampling.
  auto sample_buffers = std::exchange(this->_copied_sample_buffers, std::vector<std::vector<std::byte>>{});
  this->_copied_sample_buffers.reserve(sample_buffers.size());

  /// Copy the current mmap buffer into the buffer that will be consumed.
  if (auto buffer = this->_mmap_buffer.copy_data(); !buffer.empty()) {
    sample_buffers.push_back(std::move(buffer));
  }

  return sample_buffers;
}

void
perf::SampleBuffer::poll_and_handle_buffer_overflow(const FileDescriptorView counter_file_descriptor)
{
  do {
    /// Initialize the file descriptor set.
    auto file_descriptor_set = ::fd_set{};
    FD_ZERO(&file_descriptor_set);
    FD_SET(counter_file_descriptor.value(), &file_descriptor_set);
    FD_SET(this->_cancel_handle_overflow_thread_file_descriptor.value(), &file_descriptor_set);

    const auto max_file_descriptor =
      std::max(counter_file_descriptor.value(), this->_cancel_handle_overflow_thread_file_descriptor.value()) + 1;

    /// Block and wait for the perf file descriptor or the event file descriptor to notify.
    const auto select = ::select(max_file_descriptor, &file_descriptor_set, nullptr, nullptr, nullptr);

    if (select > 0) {
      /// If the cancel file descriptor is set, exit the loop and consequently the thread.
      if (FD_ISSET(this->_cancel_handle_overflow_thread_file_descriptor.value(), &file_descriptor_set)) {
        return;
      }

      /// If the "normal" perf file descriptor is set, drain the buffer by copying the data into an application-level
      /// buffer.
      if (FD_ISSET(counter_file_descriptor.value(), &file_descriptor_set)) {
        /// Lock the sample buffers.
        auto lock_guard = std::lock_guard(this->_buffers_mutex);

        /// Copy the ringbuffer and append to sample buffers if any data available.
        if (auto buffer = this->_mmap_buffer.copy_data(); !buffer.empty()) {
          this->_copied_sample_buffers.push_back(std::move(buffer));
        }
      }
    } else if (select == -1) {
      return;
    }
  } while (true);
}

std::uint64_t
perf::SampleBuffer::align_number_of_buffer_pages(std::uint64_t number_of_buffer_pages)
{
  /// Check if buffer pages is a power of two; if so, add one for the header as specified by the perf_event_open
  /// documentation (https://man7.org/linux/man-pages/man2/perf_event_open.2.html).
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