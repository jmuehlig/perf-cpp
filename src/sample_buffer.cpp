#include <cstring>
#include <iostream>
#include <perfcpp/hardware_info.h>
#include <perfcpp/sample_buffer.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#endif

perf::MmapBuffer::MmapBuffer(const std::int32_t file_descriptor, const bool is_write, std::uint64_t count_pages)
  : _count_pages(count_pages)
{
  /// Open the mapped buffer; use write mode if needed because a separate thread will copy data into application-level
  /// buffers.
  const auto prod_flag = PROT_READ | (static_cast<decltype(PROT_WRITE)>(is_write) * PROT_WRITE);
  this->_header = reinterpret_cast<perf_event_mmap_page*>(::mmap(nullptr,
                                                                 count_pages * HardwareInfo::memory_page_size(),
                                                                 prod_flag,
                                                                 MAP_SHARED,
                                                                 static_cast<std::int32_t>(file_descriptor),
                                                                 0));

  /// Notify the caller if buffer-allocation via ::mmap() failed.
  if (this->_header == MAP_FAILED) {
    throw MmapError{ errno };
  } else if (this->_header == nullptr) {
    throw MmapNullError{};
  }
}

perf::MmapBuffer::~MmapBuffer()
{
  /// Close/un-map the mmap-ed buffer and set number of pages to zero, if any.
  if (auto* const user_level_buffer = std::exchange(this->_header, nullptr); user_level_buffer != nullptr) {
    if (const auto user_level_buffer_pages = std::exchange(this->_count_pages, 0ULL); user_level_buffer_pages > 0ULL) {
      ::munmap(user_level_buffer, user_level_buffer_pages * HardwareInfo::memory_page_size());
    }
  }
}

std::vector<std::byte>
perf::MmapBuffer::copy_data() noexcept
{
  /// Fore more information about the perf ring buffer see:
  /// https://docs.kernel.org/userspace-api/perf_ring_buffer.html

  /// Read positions and size of the ringbuffer. Head and tail are offsets of data_start (allocated buffer + offset for
  /// metadata).

  const auto tail = this->_header->data_tail;
  const auto head = __atomic_load_n(&this->_header->data_head, __ATOMIC_ACQUIRE);
  asm volatile("" ::: "memory");

  const auto data_start = std::uintptr_t(this->_header) + HardwareInfo::memory_page_size();
#ifndef PERFCPP_NO_MMAP_DATA_SIZE /// The "data_size" attribute was added in Linux 4.1.
  const auto data_size = this->_header->data_size;
#else
  const auto data_size = (this->_count_pages - 1U) * HardwareInfo::memory_page_size();
#endif

  /// Align head and tail to the data size in case one or both are wrapped. Note: Both aligned values are offsets of
  /// data_start.
  const auto begin = head % data_size;
  const auto end = tail % data_size;

  /// Check if there is anything to read and cancel if not.
  if (begin == end) {
    return {};
  }

  /// When the tail is behind the head, we can read straightforward.
  if (end < begin) {
    /// Allocate space for the data in the buffer.
    const auto size = begin - end;
    auto buffer = std::vector<std::byte>{size};

    /// Copy the data from the tail into the buffer.
    const auto start = data_start + end;
    std::memcpy(buffer.data(), reinterpret_cast<std::byte*>(start), size);

    // Update the data_tail to the current head, marking the data as consumed.
    __sync_synchronize();
    __atomic_store_n(&this->_header->data_tail, head, __ATOMIC_RELEASE);

    return buffer;
  }

  /// When the ringbuffer wrapped inbetween, we need to copy the first part from tail to end and the second part from
  /// start to head.
  else {
    /// Allocate space for the data in the buffer.
    const auto tail_rest_size = data_size - end;
    const auto size = tail_rest_size + begin;
    auto buffer = std::vector<std::byte>{size};

    /// Copy the first part: from tail to end.
    const auto start_tail = data_start + end;
    std::memcpy(buffer.data(), reinterpret_cast<std::byte*>(start_tail), tail_rest_size);

    /// Copy the second part: from start to head.
    std::memcpy(buffer.data() + tail_rest_size, reinterpret_cast<std::byte*>(data_start), begin);

    // Update the data_tail to the current head, marking the data as consumed.
    __sync_synchronize();
    __atomic_store_n(&this->_header->data_tail, head, __ATOMIC_RELEASE);

    return buffer;
  }
}

perf::SampleBuffer::SampleBuffer(const std::int32_t file_descriptor, std::uint64_t number_of_buffer_pages)
{
  /// Align the number of buffer pages to hold a power of two + one for the header.
  number_of_buffer_pages = SampleBuffer::align_number_of_buffer_pages(number_of_buffer_pages);

  /// We only make use of buffer overflow handling (copy the data to an application-level buffer) when more than two
  /// pages are requested as two pages are allocated for live events which do not require draining the buffer.
  const auto is_use_buffer_overflow_handling = number_of_buffer_pages > 2ULL;

  /// Open the MMap Buffer.
  this->_mmap_buffer = MmapBuffer{ file_descriptor, is_use_buffer_overflow_handling, number_of_buffer_pages };

  /// Create a thread that copies data from the mmap-ed buffer into a application-level buffer whenever the buffer is
  /// near to overflow. This is only needed when overflowing is handled (which is not true for small buffers of two
  /// pages used for live counters).
  if (is_use_buffer_overflow_handling) {
    /// Create an event file descriptor to cancel the thread when closing the sample buffer.
    this->_cancel_thread_event_file_descriptor = ::eventfd(0, 0);
    if (this->_cancel_thread_event_file_descriptor.value() == -1) {
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
      this->_poll_and_handle_ringbuffer_overflow_thread.reset();
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
  if (auto buffer = this->_mmap_buffer.copy_data(); !buffer.empty()) {
    sample_buffers.push_back(std::move(buffer));
  }

  return sample_buffers;
}

std::uint64_t
perf::SampleBuffer::read_live() const noexcept
{
  /// Read the counter without stopping/disabling it via the "rdpmc" instruction.
  /// This is only possible on x86 architectures.
  /// For more details see https://man7.org/linux/man-pages/man2/perf_event_open.2.html (section MMAP layout).

#if defined(__x86_64__) || defined(__i386__)
  std::uint64_t value;
  std::uint32_t lock;

  do {
    lock = this->_mmap_buffer.lock();

    /// Memory fence.
    asm volatile("" ::: "memory");

    /// Read the hardware counter identifier.
    const auto index = this->_mmap_buffer.index();

    /// Verify that "rdpmc" is allowed.
    if (index == 0U) {
      return 0ULL;
    }

    /// Offset that must be added to the value.
    const auto offset = this->_mmap_buffer.offset();

    /// Read the value.
    value = std::uint64_t(std::int64_t(_rdpmc(index - 1U)) + offset);

    asm volatile("" ::: "memory");
  } while (this->_mmap_buffer.lock() != lock);

  return value;
#else
  return 0ULL;
#endif
}

void
perf::SampleBuffer::poll_and_handle_ringbuffer_overflow(std::int32_t perf_file_descriptor,
                                                        std::int32_t cancel_file_descriptor)
{
  do {
    /// Initialize the file descriptor set.
    auto file_descriptor_set = fd_set{};
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
        if (auto buffer = this->_mmap_buffer.copy_data(); !buffer.empty()) {
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

std::optional<perf::Metadata::Mode>
perf::SampleBuffer::Entry::mode() const noexcept
{
  const auto misc = this->_header->misc;

  if (static_cast<bool>(misc & PERF_RECORD_MISC_KERNEL)) {
    return Metadata::Mode::Kernel;
  } else if (static_cast<bool>(misc & PERF_RECORD_MISC_USER)) {
    return Metadata::Mode::User;
  } else if (static_cast<bool>(misc & PERF_RECORD_MISC_HYPERVISOR)) {
    return Metadata::Mode::Hypervisor;
  } else if (static_cast<bool>(misc & PERF_RECORD_MISC_GUEST_KERNEL)) {
    return Metadata::Mode::GuestKernel;
  } else if (static_cast<bool>(misc & PERF_RECORD_MISC_GUEST_USER)) {
    return Metadata::Mode::GuestUser;
  }

  return std::nullopt;
}