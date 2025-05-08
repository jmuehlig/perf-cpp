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

perf::SampleBuffer::SampleBuffer(const std::int32_t file_descriptor, std::uint64_t number_of_buffer_pages)
{
  /// Align the number of buffer pages to hold a power of two + one for the header.
  number_of_buffer_pages = SampleBuffer::align_number_of_buffer_pages(number_of_buffer_pages);

  /// We only make use of buffer overflow handling (copy the data to an application-level buffer) when more than two
  /// pages are requested as two pages are allocated for live events which do not require draining the buffer.
  const auto is_use_buffer_overflow_handling = number_of_buffer_pages > 2ULL;

  /// Open the mapped buffer; use write mode if needed because a separate thread will copy data into application-level
  /// buffers.
  const auto prod_flag = PROT_READ | (is_use_buffer_overflow_handling ? PROT_WRITE : 0x0);
  this->_mmap_ringbuffer =
    reinterpret_cast<perf_event_mmap_page*>(::mmap(nullptr,
                                                   number_of_buffer_pages * HardwareInfo::memory_page_size(),
                                                   prod_flag,
                                                   MAP_SHARED,
                                                   static_cast<std::int32_t>(file_descriptor),
                                                   0));

  /// Notify the caller if buffer-allocation via ::mmap() failed.
  if (this->_mmap_ringbuffer == MAP_FAILED) {
    throw MmapError{ errno };
  } else if (this->_mmap_ringbuffer == nullptr) {
    throw MmapNullError{};
  }

  /// If the ringbuffer was opened successfully, remember the number of pages in order to unmap when closing the
  /// counter.
  this->_count_pages = number_of_buffer_pages;

  /// Create a thread that copies data from the mmap-ed buffer into a application-level buffer whenever the buffer is
  /// near to overflow. This is only needed when overflowing is handled (which is not true for small buffers of two
  /// pages used for live counters).
  if (is_use_buffer_overflow_handling) {
    /// Create an event file descriptor to cancel the thread when closing the sample buffer.
    this->_cancel_thread_event_file_descriptor = ::eventfd(0, 0);
    if (this->_cancel_thread_event_file_descriptor.value() == -1) {
      throw CannotCreateEventFileDescriptor{ file_descriptor };
    }

    /// Allocate some space for application-level buffers.
    this->_application_buffers.reserve(32U);

    /// Create the handle thread.
    this->_poll_and_handle_ringbuffer_overflow_thread = std::thread(&SampleBuffer::poll_for_ringbuffer_overflow,
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

  /// Close/un-map the mmap-ed buffer and set number of pages to zero, if any.
  if (auto* const user_level_buffer = std::exchange(this->_mmap_ringbuffer, nullptr); user_level_buffer != nullptr) {
    if (const auto user_level_buffer_pages = std::exchange(this->_count_pages, 0ULL); user_level_buffer_pages > 0ULL) {
      ::munmap(user_level_buffer, user_level_buffer_pages * HardwareInfo::memory_page_size());
    }
  }
}

std::vector<std::pair<std::uintptr_t, std::uintptr_t>>
perf::SampleBuffer::buffer_ranges() const
{
  /// List of (start, end) pointers for different buffers (application-level and mmap-ed ringbuffer).
  auto iterators = std::vector<std::pair<std::uintptr_t, std::uintptr_t>>{};
  iterators.reserve(this->_application_buffers.size() + /* space for mmap iterators */ 2U);

  /// Add the data from the buffers containing the data whenever the mmap-ed buffer was near to overflowing.
  for (const auto& buffer : this->_application_buffers) {
    iterators.emplace_back(std::uintptr_t(buffer.data()), std::uintptr_t(buffer.data() + buffer.size()) - 1U);
  }

  /// Add the mmap-ed buffer.
  if (this->_mmap_ringbuffer != nullptr) {
    asm volatile("" ::: "memory");
    const auto tail = this->_mmap_ringbuffer->data_tail;
    const auto head = this->_mmap_ringbuffer->data_head;

    /// Read size and start position.
#ifndef PERFCPP_NO_MMAP_DATA_SIZE /// The "data_size" attribute was added in Linux 4.1.
    const auto data_size = this->_mmap_ringbuffer->data_size;
#else
    const auto data_size = (this->_count_pages - 1U) * HardwareInfo::memory_page_size();
#endif
    const auto data_start = std::uintptr_t(this->_mmap_ringbuffer) + HardwareInfo::memory_page_size();

    /// Align head and tail to the data size in case one or both are wrapped.
    const auto begin = head % data_size;
    const auto end = tail % data_size;

    /// If the buffer is empty, we are done.
    if (begin == end) {
      return iterators;
    }

    /// When the tail is behind the head, we can read the samples straightforward.
    if (end < begin) {
      const auto start = data_start + end;
      const auto size = begin - end;

      iterators.emplace_back(start, start + size);
    }

    /// When the ringbuffer wrapped inbetween, we need to access the first part from tail to end and the second part
    /// from start to head.
    else {
      /// 1st: Add an iterator from tail to buffer end.
      const auto tail_rest_size = data_size - end;
      const auto start_tail = data_start + end;
      iterators.emplace_back(start_tail, start_tail + tail_rest_size);

      /// 2nd: Add an iterator from data start to head.
      iterators.emplace_back(data_start, begin);
    }
  }

  return iterators;
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
    lock = this->_mmap_ringbuffer->lock;

    /// Memory fence.
    asm volatile("" ::: "memory");

    /// Read the hardware counter identifier.
    const auto index = this->_mmap_ringbuffer->index;

    /// Verify that "rdpmc" is allowed.
    if (index == 0U) {
      return 0ULL;
    }

    /// Offset that must be added to the value.
    const auto offset = this->_mmap_ringbuffer->offset;

    /// Read the value.
    value = std::uint64_t(std::int64_t(_rdpmc(index - 1U)) + offset);

    asm volatile("" ::: "memory");
  } while (this->_mmap_ringbuffer->lock != lock);

  return value;
#else
  return 0ULL;
#endif
}

void
perf::SampleBuffer::poll_for_ringbuffer_overflow(const std::int32_t perf_file_descriptor,
                                                 const std::int32_t cancel_file_descriptor)
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
        if (this->_mmap_ringbuffer != nullptr) {
          this->copy_perf_ringbuffer_into_application_buffer();
        } else {
          /// Cancel when the ringbuffer is deallocated.
          return;
        }
      }
    } else if (ret == -1) {
      return;
    }
  } while (true);
}

void
perf::SampleBuffer::copy_perf_ringbuffer_into_application_buffer()
{
  /// Fore more information about the perf ring buffer see:
  /// https://docs.kernel.org/userspace-api/perf_ring_buffer.html

  /// Read positions and size of the ringbuffer. Head and tail are offsets of data_start (allocated buffer + offset for
  /// metadata).
  asm volatile("" ::: "memory");
  const auto tail = this->_mmap_ringbuffer->data_tail;
  const auto head = __atomic_load_n(&this->_mmap_ringbuffer->data_head, __ATOMIC_ACQUIRE);

  const auto data_start = std::uintptr_t(this->_mmap_ringbuffer) + HardwareInfo::memory_page_size();
#ifndef PERFCPP_NO_MMAP_DATA_SIZE /// The "data_size" attribute was added in Linux 4.1.
  const auto data_size = this->_mmap_ringbuffer->data_size;
#else
  const auto data_size = (this->_count_pages - 1U) * HardwareInfo::memory_page_size();
#endif

  /// Align head and tail to the data size in case one or both are wrapped. Note: Both aligned values are offsets of
  /// data_start.
  const auto begin = head % data_size;
  const auto end = tail % data_size;

  /// Check if there is anything to read and cancel if not.
  if (begin == end) {
    return;
  }

  /// Application-level buffer where the data from the ringbuffer is copied to.
  auto buffer = std::vector<std::byte>{};

  /// When the tail is behind the head, we can read straightforward.
  if (end < begin) {
    /// Allocate space for the data in the buffer.
    const auto size = begin - end;
    buffer.resize(size);

    /// Copy the data from the tail into the buffer.
    const auto start = data_start + end;
    std::memcpy(buffer.data(), reinterpret_cast<std::byte*>(start), size);
  }

  /// When the ringbuffer wrapped inbetween, we need to copy the first part from tail to end and the second part from
  /// start to head.
  else {
    /// Allocate space for the data in the buffer.
    const auto tail_rest_size = data_size - end;
    const auto size = tail_rest_size + begin;
    buffer.resize(size);

    /// Copy the first part: from tail to end.
    const auto start_tail = data_start + end;
    std::memcpy(buffer.data(), reinterpret_cast<std::byte*>(start_tail), tail_rest_size);

    /// Copy the second part: from start to head.
    std::memcpy(buffer.data() + tail_rest_size, reinterpret_cast<std::byte*>(data_start), begin);
  }

  // Update the data_tail to the current head, marking the data as consumed.
  __atomic_store_n(&this->_mmap_ringbuffer->data_tail, head, __ATOMIC_RELEASE);

  /// Add the buffer to the list of buffers.
  this->_application_buffers.push_back(std::move(buffer));
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