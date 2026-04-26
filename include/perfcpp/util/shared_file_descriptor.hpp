#pragma once

#include <atomic>
#include <cstdint>
#include <unistd.h>
#include <utility>

namespace perf::util {
/**
 * The shared file descriptor wraps a file descriptor and allows sharing between multiple instances
 * using atomic reference counting. Closing happens when the last owner is destroyed – comparable to
 * a shared pointer.
 */
class SharedFileDescriptor
{
public:
  SharedFileDescriptor() noexcept = default;

  explicit SharedFileDescriptor(const int file_descriptor)
    : _ref_count(new std::atomic<std::uint64_t>{ 1U })
    , _file_descriptor(file_descriptor)
  {
  }

  SharedFileDescriptor(const SharedFileDescriptor& other) noexcept
    : _ref_count(other._ref_count)
    , _file_descriptor(other._file_descriptor)
  {
    if (_ref_count != nullptr) {
      ++(*_ref_count);
    }
  }

  SharedFileDescriptor(SharedFileDescriptor&& other) noexcept
    : _ref_count(std::exchange(other._ref_count, nullptr))
    , _file_descriptor(std::exchange(other._file_descriptor, -1))
  {
  }

  /**
   * Decrements the reference count and closes the file descriptor when the last owner is destroyed.
   */
  ~SharedFileDescriptor()
  {
    if (_ref_count != nullptr && --(*_ref_count) == 0U) {
      ::close(_file_descriptor);
      delete _ref_count;
    }
  }

  SharedFileDescriptor& operator=(const SharedFileDescriptor& other) noexcept
  {
    if (this != &other) {
      if (_ref_count != nullptr && --(*_ref_count) == 0U) {
        ::close(_file_descriptor);
        delete _ref_count;
      }
      _ref_count = other._ref_count;
      _file_descriptor = other._file_descriptor;
      if (_ref_count != nullptr) {
        ++(*_ref_count);
      }
    }
    return *this;
  }

  SharedFileDescriptor& operator=(SharedFileDescriptor&& other) noexcept
  {
    if (this != &other) {
      if (_ref_count != nullptr && --(*_ref_count) == 0U) {
        ::close(_file_descriptor);
        delete _ref_count;
      }
      _ref_count = std::exchange(other._ref_count, nullptr);
      _file_descriptor = std::exchange(other._file_descriptor, -1);
    }
    return *this;
  }

  /**
   * @return True, if the file descriptor underneath is opened.
   */
  [[nodiscard]] bool has_value() const noexcept { return _ref_count != nullptr; }

  /**
   * @return The "real" file descriptor.
   */
  [[nodiscard]] int value() const noexcept { return _file_descriptor; }

private:
  /// Heap-allocated reference count; nullptr indicates an empty (unowned) state.
  std::atomic<std::uint64_t>* _ref_count{ nullptr };
  int _file_descriptor{ -1 };
};
}
