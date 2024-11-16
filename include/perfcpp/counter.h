#pragma once

#include "config.h"
#include <array>
#include <cstdint>
#include <linux/perf_event.h>
#include <optional>
#include <perfcpp/precision.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace perf {
class CounterConfig
{
public:
  CounterConfig(const std::uint32_t type,
                const std::uint64_t event_id,
                const std::uint64_t event_id_extension_1 = 0U,
                const std::uint64_t event_id_extension_2 = 0U) noexcept
    : _type(type)
    , _event_id(event_id)
    , _event_id_extension({ event_id_extension_1, event_id_extension_2 })
  {
  }

  ~CounterConfig() noexcept = default;

  void precise_ip(const std::uint8_t precise_ip) noexcept { _precise_ip = precise_ip; }
  void period_or_frequency(const PeriodOrFrequency period_or_frequency) noexcept
  {
    _period_or_frequency = period_or_frequency;
  }

  [[nodiscard]] std::uint32_t type() const noexcept { return _type; }
  [[nodiscard]] std::uint64_t event_id() const noexcept { return _event_id; }
  [[nodiscard]] std::array<std::uint64_t, 2U> event_id_extension() const noexcept { return _event_id_extension; }
  [[nodiscard]] std::optional<std::uint8_t> precise_ip() const noexcept { return _precise_ip; }
  [[nodiscard]] std::optional<PeriodOrFrequency> period_or_frequency() const noexcept { return _period_or_frequency; }

private:
  std::uint32_t _type;
  std::uint64_t _event_id;
  std::array<std::uint64_t, 2U> _event_id_extension;
  std::optional<std::uint8_t> _precise_ip{ std::nullopt };
  std::optional<PeriodOrFrequency> _period_or_frequency{ std::nullopt };
};

class CounterResult
{
public:
  using iterator = std::vector<std::pair<std::string_view, double>>::iterator;
  using const_iterator = std::vector<std::pair<std::string_view, double>>::const_iterator;

  CounterResult() = default;
  CounterResult(CounterResult&&) noexcept = default;
  CounterResult(const CounterResult&) = default;
  explicit CounterResult(std::vector<std::pair<std::string_view, double>>&& results) noexcept
    : _results(std::move(results))
  {
  }

  ~CounterResult() = default;

  CounterResult& operator=(CounterResult&&) noexcept = default;
  CounterResult& operator=(const CounterResult&) = default;

  /**
   * Access the result of the counter or metric with the given name.
   *
   * @param name Name of the counter or metric to access.
   * @return The value, or std::nullopt of the result has no counter or value with the requested name.
   */
  [[nodiscard]] std::optional<double> get(std::string_view name) const noexcept;

  [[nodiscard]] iterator begin() { return _results.begin(); }
  [[nodiscard]] iterator end() { return _results.end(); }
  [[nodiscard]] const_iterator begin() const { return _results.begin(); }
  [[nodiscard]] const_iterator end() const { return _results.end(); }

  /**
   * Converts the result to a json-formatted string.
   * @return Result in JSON format.
   */
  [[nodiscard]] std::string to_json() const;

  /**
   * Converts the result to a CSV-formatted string.
   *
   * @param delimiter Char to separate columns (',' by default).
   * @param print_header If true, the header will be printed first (true by default).
   * @return Result in CSV format.
   */
  [[nodiscard]] std::string to_csv(char delimiter = ',', bool print_header = true) const;

  /**
   * Converts the result to a table-formatted string.
   * @return Result as a table-formatted string.
   */
  [[nodiscard]] std::string to_string() const;

private:
  std::vector<std::pair<std::string_view, double>> _results;
};

class Counter
{
public:
  explicit Counter(CounterConfig config) noexcept
    : _config(config)
  {
  }

  ~Counter();

  /**
   * @return ID of the counter.
   */
  [[nodiscard]] std::uint64_t id() const noexcept { return _id; }

  /**
   * @return The file descriptor of the counter; -1 if the counter was not opened (successfully).
   */
  [[nodiscard]] std::int64_t file_descriptor() const noexcept { return _file_descriptor; }

  /**
   * Opens the counter using the perf subsystem via the perf_event_open system call.
   * The counter will be configured with the provided parameters.
   * After successfully open the counter, the counter's file descriptor will be set.
   * If the counter cannot be opened, it will throw an exception including the error number.
   *
   * @param config Configuration.
   * @param is_group_leader True, if this counter is the group leader.
   * @param is_secret_leader True, if this counter is not the group leader but the group leader is an auxiliary counter.
   * @param group_leader_file_descriptor File descriptor of the group leader; may be -1 (or any other –unused– value),
   * if this is the group leader.
   * @param is_read_format True, if counters should be read.
   * @param buffer_pages Number of pages allocated for user-level buffer, std::nullopt if counter should not allocated
   * any pages.
   * @param sample_type Mask of sampled values, std::nullopt of sampling is disabled.
   * @param branch_type Mask of sampled branch types, std::nullopt of sampling is disabled.
   * @param user_registers Mask of sampled user registers, std::nullopt of sampling is disabled.
   * @param kernel_registers Mask of sampled kernel registers, std::nullopt of sampling is disabled.
   * @param max_user_stack_size Maximal size of sampled user stack, std::nullopt of sampling is disabled.
   * @param max_callstack_size Maximal size of sampled callstacks, std::nullopt of sampling is disabled.
   * @param is_include_context_switch True, if context switches should be sampled, ignored if sampling is disabled.
   * @param is_include_cgroup True, if cgroups should be sampled, ignored if sampling is disabled.
   */
  void open(const perf::Config& config,
            bool is_group_leader,
            bool is_secret_leader,
            std::int64_t group_leader_file_descriptor,
            bool is_read_format,
            std::optional<std::uint64_t> buffer_pages,
            std::optional<std::uint64_t> sample_type,
            std::optional<std::uint64_t> branch_type,
            std::optional<std::uint64_t> user_registers,
            std::optional<std::uint64_t> kernel_registers,
            std::optional<std::uint32_t> max_user_stack_size,
            std::optional<std::uint16_t> max_callstack_size,
            bool is_include_context_switch,
            bool is_include_cgroup);

  /**
   * Closes the counter and resets the file descriptor.
   */
  void close();

  /**
   * Enables the counter.
   */
  void enable() const;

  /**
   * Disables the counter.
   */
  void disable() const;

  /**
   * Reads the counter "live" without stopping via the "rdpmc" instruction.
   * Note that this is only possible on x86 architectures.
   *
   * @return The current value of the counter.
   */
  [[nodiscard]] std::uint64_t read_live() const noexcept;

  /**
   * @return First page of the user-level buffer.
   */
  [[nodiscard]] perf_event_mmap_page* user_level_buffer() const noexcept { return _user_level_buffer; }

  /**
   * @return A string representing all configurations of this counter.
   */
  [[nodiscard]] std::string to_string(std::optional<bool> is_group_leader = std::nullopt,
                                      std::optional<std::int64_t> group_leader_file_descriptor = std::nullopt,
                                      std::optional<pid_t> process_id = std::nullopt,
                                      std::optional<std::int32_t> cpu_id = std::nullopt) const;

  [[nodiscard]] bool operator==(const CounterConfig config) const noexcept
  {
    return _config.type() == config.type() && _config.event_id() == config.event_id();
  }

private:
  /// The config of an event.
  CounterConfig _config;

  /// The event description for the perf subsystem, will be configured when opening.
  perf_event_attr _event_attribute{};

  /// Id of an event, will be set by ::ioctl and is needed to match counter values from the perf reading format.
  std::uint64_t _id{ 0U };

  /// The file descriptor as returned by the perf subsystem when opening the counter.
  std::int64_t _file_descriptor{ -1 };

  /// Some counter (e.g., triggers for sampling and those read with the rdpmc instruction) use mmap-ed buffers.
  perf_event_mmap_page* _user_level_buffer{ nullptr };

  /// Number of pages allocated for the user level buffer (needed for closing the buffer).
  std::optional<std::uint64_t> _user_level_buffer_pages{ std::nullopt };

  /**
   * Do the "final" perf_event_open system call with the provided parameters.
   *
   * @param process_id ID of the process to monitor.
   * @param cpu_id ID of the CPU to monitor.
   * @param is_group_leader True, if this counter is the group leader.
   * @param group_leader_file_descriptor File descriptor of the group leader.
   * @return The file descriptor, which is returned by the system call (-1 in case the call was not successful).
   */
  std::int64_t perf_event_open(pid_t process_id,
                               std::int32_t cpu_id,
                               bool is_group_leader,
                               std::int64_t group_leader_file_descriptor);

  /**
   * Decides whether adjusting (i.e., decrementing) the precise_ip configuration could help to open a hardware
   * performance counter if an previous attempt failed. This is only true for sampling, if the current precise_ip is too
   * high and the error code indicates to do so (e.g., reporting an invalid argument).
   *
   * @param current_precise_ip The current value of the precise_ip configuration.
   * @param sample_type Sample type; std::nullopt if opened for counting only.
   * @param error_code The error code when failing.
   * @return True, when the counter should try to open again.
   */
  [[nodiscard]] static bool is_adjust_precise_ip(std::uint8_t current_precise_ip,
                                                 std::optional<std::uint64_t> sample_type,
                                                 std::int64_t error_code) noexcept;

  /**
   * Creates an exception message based on the errno set when accessing the perf subsystem to open an event.
   *
   * @param error_code Error code raised when calling perf_event_open.
   * @return Error message that can be thrown to inform the user.
   */
  [[nodiscard]] static std::string error_message_from_errno(std::int64_t error_code);

  /**
   * Prints a name of a type (e.g., sample, branch, ...) to the stream if the type is set in the mask.
   *
   * @param stream Stream to print the name of the type on.
   * @param mask Given mask to check if the type is set.
   * @param type Type to test in the mask.
   * @param name Name to print if the type is set.
   * @param is_need_print_delimiter Flag if this is the first printed name. If false, a delimiter is printed in front of
   * the name.
   * @return True, if this was the first printed type.
   */
  static bool print_type_to_stream(std::stringstream& stream,
                                   std::uint64_t mask,
                                   std::uint64_t type,
                                   std::string&& name,
                                   bool is_need_print_delimiter);
};
}