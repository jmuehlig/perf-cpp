#include <perfcpp/mmap_buffer.h>
#include <cstring>
#include <sys/mman.h>
#include <perfcpp/exception.h>
#include <perfcpp/hardware_info.h>

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#endif

perf::MmapBuffer::MmapBuffer(const std::int32_t file_descriptor, const bool is_write, std::uint64_t count_pages)
  : _count_pages(count_pages)
{
  /// Open the mapped buffer; use write mode if needed because a separate thread will copy data into application-level
  /// buffers.
  const auto prod_flags = PROT_READ | (static_cast<decltype(PROT_WRITE)>(is_write) * PROT_WRITE);
  this->_header = reinterpret_cast<perf_event_mmap_page*>(::mmap(nullptr,
                                                                 count_pages * HardwareInfo::memory_page_size(),
                                                                 prod_flags,
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

std::uint64_t
perf::MmapBuffer::read_performance_monitoring_counter() const noexcept
{
  /// Read the counter without stopping/disabling it via the "rdpmc" instruction.
  /// This is only possible on x86 architectures.
  /// For more details see https://man7.org/linux/man-pages/man2/perf_event_open.2.html (section MMAP layout).

#if defined(__x86_64__) || defined(__i386__)
  std::uint64_t value;
  decltype(perf_event_mmap_page::lock) lock;

  do {
    lock = this->_header->lock;

    /// Memory fence.
    asm volatile("" ::: "memory");

    /// Read the hardware counter identifier.
    const auto index = this->_header->index;

    /// Verify that "rdpmc" is allowed.
    if (index == 0U) {
      return 0ULL;
    }

    /// Offset that must be added to the value.
    const auto offset = this->_header->offset;

    /// Read the value.
    value = std::uint64_t(std::int64_t(_rdpmc(index - 1U)) + offset);

    asm volatile("" ::: "memory");
  } while (this->_header->lock != lock);

  return value;
#else
  return 0ULL;
#endif
}

std::vector<std::byte>
perf::MmapBuffer::copy_sample_data() noexcept
{
  /// Fore more information about the perf ring buffer see:
  /// https://docs.kernel.org/userspace-api/perf_ring_buffer.html

  /// Read positions and size of the ringbuffer. Head and tail are offsets of data_start (allocated buffer + offset for
  /// metadata).
  const auto tail = this->_header->data_tail;
  const auto head = __atomic_load_n(&this->_header->data_head, __ATOMIC_ACQUIRE);
  asm volatile("" ::: "memory");

  /// Check if there is anything to read and cancel if not.
  if (tail == head) {
    return {};
  }

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

  /// When the tail is behind the head, we can read straightforward.
  if (end < begin) {
    /// Allocate space for the data in the buffer.
    const auto size = begin - end;
    auto buffer = std::vector<std::byte>(size);

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
    auto buffer = std::vector<std::byte>(size);

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
