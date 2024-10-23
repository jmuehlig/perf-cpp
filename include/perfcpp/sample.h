#pragma once

#include "counter.h"
#include "data_source.h"
#include "branch.h"
#include "weight.h"
#include "transaction.h"
#include <cstdint>
#include <linux/perf_event.h>
#include <optional>

namespace perf {

class CGroup
{
public:
  CGroup(const std::uint64_t id, std::string&& path) noexcept
    : _id(id)
    , _path(std::move(path))
  {
  }
  ~CGroup() = default;

  /**
   * @return Id of the CGgroup (as found in samples).
   */
  [[nodiscard]] std::uint64_t id() const noexcept { return _id; }

  /**
   * @return Path of the CGroup.
   */
  [[nodiscard]] const std::string& path() const noexcept { return _path; }

private:
  std::uint64_t _id;
  std::string _path;
};

class ContextSwitch
{
public:
  ContextSwitch(const bool is_out,
                const bool is_preempt,
                const std::optional<std::uint32_t> process_id,
                const std::optional<std::uint32_t> thread_id) noexcept
    : _is_out(is_out)
    , _is_preempt(is_preempt)
    , _process_id(process_id)
    , _thread_id(thread_id)
  {
  }
  ~ContextSwitch() noexcept = default;

  /**
   * @return True, if the process/thread was switched out.
   */
  [[nodiscard]] bool is_out() const noexcept { return _is_out; }

  /**
   * @return True, if the process/thread was switched in.
   */
  [[nodiscard]] bool is_in() const noexcept { return !_is_out; }

  /**
   * @return True, if the process/thread was preempted.
   */
  [[nodiscard]] bool is_preempt() const noexcept { return _is_preempt; }

  /**
   * @return Id of the process, or std::nullopt if not provided (currently only provided on CPU-wide sampling).
   */
  [[nodiscard]] std::optional<std::uint32_t> process_id() const noexcept { return _process_id; }

  /**
   * @return Id of the thread, or std::nullopt if not provided (currently only provided on CPU-wide sampling).
   */
  [[nodiscard]] std::optional<std::uint32_t> thread_id() const noexcept { return _thread_id; }

private:
  bool _is_out;
  bool _is_preempt;
  std::optional<std::uint32_t> _process_id{ std::nullopt };
  std::optional<std::uint32_t> _thread_id{ std::nullopt };
};

class Throttle
{
public:
  explicit Throttle(const bool is_throttle) noexcept
    : _is_throttle(is_throttle)
  {
  }
  ~Throttle() noexcept = default;

  /**
   * @return True, if the event was a throttle event.
   */
  [[nodiscard]] bool is_throttle() const noexcept { return _is_throttle; }

  /**
   * @return True, if the event was an unthrottle event.
   */
  [[nodiscard]] bool is_unthrottle() const noexcept { return !_is_throttle; }

private:
  bool _is_throttle;
};

class Sample
{
public:
  enum Mode
  {
    Unknown,
    Kernel,
    User,
    Hypervisor,
    GuestKernel,
    GuestUser
  };

  explicit Sample(Mode mode) noexcept
    : _mode(mode)
  {
  }
  ~Sample() noexcept = default;

  void sample_id(const std::uint64_t sample_id) noexcept { _sample_id = sample_id; }
  void instruction_pointer(const std::uintptr_t instruction_pointer) noexcept
  {
    _instruction_pointer = instruction_pointer;
  }
  void process_id(const std::uint32_t process_id) noexcept { _process_id = process_id; }
  void thread_id(const std::uint32_t thread_id) noexcept { _thread_id = thread_id; }
  void timestamp(const std::uint64_t timestamp) noexcept { _time = timestamp; }
  void stream_id(const std::uint64_t stream_id) noexcept { _stream_id = stream_id; }
  void raw(std::vector<char>&& raw) noexcept { _raw_data = raw; }
  void logical_memory_address(const std::uintptr_t logical_memory_address) noexcept
  {
    _logical_memory_address = logical_memory_address;
  }
  void physical_memory_address(const std::uintptr_t physical_memory_address) noexcept
  {
    _physical_memory_address = physical_memory_address;
  }
  void id(const std::uint64_t id) noexcept { _id = id; }
  void cpu_id(const std::uint32_t cpu_id) noexcept { _cpu_id = cpu_id; }
  void period(const std::uint64_t period) noexcept { _period = period; }
  void counter_result(CounterResult&& counter_result) noexcept { _counter_result = std::move(counter_result); }
  void data_src(const DataSource data_src) noexcept { _data_src = data_src; }
  void transaction_abort(const TransactionAbort transaction_abort) noexcept { _transaction_abort = transaction_abort; }
  void weight(const Weight weight) noexcept { _weight = weight; }
  void branches(std::vector<Branch>&& branches) noexcept { _branches = std::move(branches); }
  void user_registers_abi(const std::uint64_t abi) noexcept { _user_registers_abi = abi; }
  void user_registers(std::vector<std::uint64_t>&& user_registers) noexcept
  {
    _user_registers = std::move(user_registers);
  }
  void kernel_registers_abi(const std::uint64_t abi) noexcept { _kernel_registers_abi = abi; }
  void kernel_registers(std::vector<std::uint64_t>&& kernel_registers) noexcept
  {
    _kernel_registers = std::move(kernel_registers);
  }
  void callchain(std::vector<std::uintptr_t>&& callchain) noexcept { _callchain = std::move(callchain); }
  void cgroup_id(const std::uint64_t cgroup_id) noexcept { _cgroup_id = cgroup_id; }
  void data_page_size(const std::uint64_t size) noexcept { _data_page_size = size; }
  void code_page_size(const std::uint64_t size) noexcept { _code_page_size = size; }
  void count_loss(const std::uint64_t count_loss) noexcept { _count_loss = count_loss; }
  void cgroup(CGroup&& cgroup) noexcept { _cgroup = std::move(cgroup); }
  void context_switch(ContextSwitch&& context_switch) noexcept { _context_switch = context_switch; }
  void throttle(Throttle&& throttle) noexcept { _throttle = throttle; }
  void is_exact_ip(const bool is_exact_ip) noexcept { _is_exact_ip = is_exact_ip; }

  /*
   * Returns the mode in which the sample was taken (e.g., Kernel, User, Hypervisor).
   * @return The sample mode.
   */
  [[nodiscard]] Mode mode() const noexcept { return _mode; }

  /*
   * Retrieves the unique identifier for the sample.
   * @return An optional containing the sample ID if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> sample_id() const noexcept { return _sample_id; }

  /*
   * Retrieves the instruction pointer at the time the sample was recorded.
   * @return An optional containing the instruction pointer address if available.
   */
  [[nodiscard]] std::optional<std::uintptr_t> instruction_pointer() const noexcept { return _instruction_pointer; }

  /*
   * Retrieves the process ID associated with the sample.
   * @return An optional containing the process ID if available.
   */
  [[nodiscard]] std::optional<std::uint32_t> process_id() const noexcept { return _process_id; }

  /*
   * Retrieves the thread ID associated with the sample.
   * @return An optional containing the thread ID if available.
   */
  [[nodiscard]] std::optional<std::uint32_t> thread_id() const noexcept { return _thread_id; }

  /*
   * Retrieves the timestamp when the sample was taken.
   * @return An optional containing the timestamp if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> time() const noexcept { return _time; }

  /*
   * Retrieves the stream id.
   * @return An optional containing the stream id if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> stream_id() const noexcept { return _stream_id; }

  /*
   * Retrieves raw data.
   * @return An optional containing raw data if available.
   */
  [[nodiscard]] const std::optional<std::vector<char>>& raw() const noexcept { return _raw_data; }

  /*
   * Retrieves the logical (virtual) memory address relevant to the sample.
   * @return An optional containing the logical memory address if available.
   */
  [[nodiscard]] std::optional<std::uintptr_t> logical_memory_address() const noexcept
  {
    return _logical_memory_address;
  }

  /*
   * Retrieves the physical memory address relevant to the sample.
   * @return An optional containing the physical memory address if available.
   */
  [[nodiscard]] std::optional<std::uintptr_t> physical_memory_address() const noexcept
  {
    return _physical_memory_address;
  }

  /*
   * Retrieves the unique ID of the perf_event that generated the sample.
   * @return An optional containing the perf_event ID if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> id() const noexcept { return _id; }

  /*
   * Retrieves the CPU ID where the sample was collected.
   * @return An optional containing the CPU ID if available.
   */
  [[nodiscard]] std::optional<std::uint32_t> cpu_id() const noexcept { return _cpu_id; }

  /*
   * Retrieves the period value indicating the number of events that have occurred.
   * @return An optional containing the period value if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> period() const noexcept { return _period; }

  /*
   * Retrieves the counter result associated with the sample.
   * @return An optional containing the counter result if available.
   */
  [[nodiscard]] const std::optional<CounterResult>& counter_result() const noexcept { return _counter_result; }

  /*
   * Retrieves the counter result associated with the sample.
   * @return An optional containing the counter result if available.
   */
  [[nodiscard]] const std::optional<CounterResult>& counter() const noexcept { return _counter_result; }

  /*
   * Retrieves the data source information of the sample.
   * @return An optional containing the data source if available.
   */
  [[nodiscard]] std::optional<DataSource> data_src() const noexcept { return _data_src; }

  /*
   * Retrieves the transaction abort of the sample.
   * @return An optional containing the transaction abort if available.
   */
  [[nodiscard]] std::optional<TransactionAbort> transaction_abort() const noexcept { return _transaction_abort; }

  /*
   * Retrieves the weight value representing the cost or impact of the sample.
   * @return An optional containing the weight if available.
   */
  [[nodiscard]] std::optional<Weight> weight() const noexcept { return _weight; }

  /*
   * Retrieves the branches recorded in the sample.
   * @return An optional vector of branches if available.
   */
  [[nodiscard]] const std::optional<std::vector<Branch>>& branches() const noexcept { return _branches; }

  /*
   * Retrieves the branches recorded in the sample (modifiable).
   * @return An optional vector of branches if available.
   */
  [[nodiscard]] std::optional<std::vector<Branch>>& branches() noexcept { return _branches; }

  /*
   * Retrieves the ABI of the user-space registers.
   * @return An optional containing the user registers ABI if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> user_registers_abi() const noexcept { return _user_registers_abi; }

  /*
   * Retrieves the user-space registers captured in the sample.
   * @return An optional vector of user registers if available.
   */
  [[nodiscard]] const std::optional<std::vector<std::uint64_t>>& user_registers() const noexcept
  {
    return _user_registers;
  }

  /*
   * Retrieves the user-space registers captured in the sample (modifiable).
   * @return An optional vector of user registers if available.
   */
  [[nodiscard]] std::optional<std::vector<std::uint64_t>>& user_registers() noexcept { return _user_registers; }

  /*
   * Retrieves the ABI of the kernel-space registers.
   * @return An optional containing the kernel registers ABI if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> kernel_registers_abi() const noexcept { return _kernel_registers_abi; }

  /*
   * Retrieves the kernel-space registers captured in the sample.
   * @return An optional vector of kernel registers if available.
   */
  [[nodiscard]] const std::optional<std::vector<std::uint64_t>>& kernel_registers() const noexcept
  {
    return _kernel_registers;
  }

  /*
   * Retrieves the kernel-space registers captured in the sample (modifiable).
   * @return An optional vector of kernel registers if available.
   */
  [[nodiscard]] std::optional<std::vector<std::uint64_t>>& kernel_registers() noexcept { return _kernel_registers; }

  /*
   * Retrieves the call chain (stack backtrace) captured in the sample.
   * @return An optional vector of instruction pointers if available.
   */
  [[nodiscard]] const std::optional<std::vector<std::uintptr_t>>& callchain() const noexcept { return _callchain; }

  /*
   * Retrieves the call chain (stack backtrace) captured in the sample (modifiable).
   * @return An optional vector of instruction pointers if available.
   */
  [[nodiscard]] std::optional<std::vector<std::uintptr_t>>& callchain() noexcept { return _callchain; }

  /*
   * Retrieves the cgroup ID of the for the perf_event subsystem.
   * @return An optional containing the cgroup ID if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> cgroup_id() const noexcept { return _cgroup_id; }

  /*
   * Retrieves the cgroup. CGroups are recorded when created and activated.
   * @return An optional containing the cgroup if available.
   */
  [[nodiscard]] const std::optional<CGroup>& cgroup() const noexcept { return _cgroup; }

  /*
   * Retrieves the data page size at the time of the sample.
   * @return An optional containing the data page size if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> data_page_size() const noexcept { return _data_page_size; }

  /*
   * Retrieves the code page size at the time of the sample.
   * @return An optional containing the code page size if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> code_page_size() const noexcept { return _code_page_size; }

  /*
   * Retrieves the context switch.
   * @return An optional containing the context switch if available.
   */
  [[nodiscard]] std::optional<ContextSwitch> context_switch() const noexcept { return _context_switch; }

  /*
   * Retrieves the count of lost events associated with the sample.
   * @return An optional containing the count of lost events if available.
   */
  [[nodiscard]] std::optional<std::uint64_t> count_loss() const noexcept { return _count_loss; }

  /*
   * Retrieves a throttle/unthrottle event.
   * @return An optional containing a throttle or an unthrottle flag if available.
   */
  [[nodiscard]] std::optional<Throttle> throttle() const noexcept { return _throttle; }

  /*
   * Indicates whether the instruction pointer in the sample is exact.
   * @return True if the instruction pointer is exact; otherwise, false.
   */
  [[nodiscard]] bool is_exact_ip() const noexcept { return _is_exact_ip; }

private:
  Mode _mode;
  std::optional<std::uint64_t> _sample_id{ std::nullopt };
  std::optional<std::uintptr_t> _instruction_pointer{ std::nullopt };
  std::optional<std::uint32_t> _process_id{ std::nullopt };
  std::optional<std::uint32_t> _thread_id{ std::nullopt };
  std::optional<std::uint64_t> _time{ std::nullopt };
  std::optional<std::uint64_t> _stream_id{ std::nullopt };
  std::optional<std::vector<char>> _raw_data{ std::nullopt };
  std::optional<std::uintptr_t> _logical_memory_address{ std::nullopt };
  std::optional<std::uintptr_t> _physical_memory_address{ std::nullopt };
  std::optional<std::uint64_t> _id{ std::nullopt };
  std::optional<std::uint32_t> _cpu_id{ std::nullopt };
  std::optional<std::uint64_t> _period{ std::nullopt };
  std::optional<CounterResult> _counter_result{ std::nullopt };
  std::optional<DataSource> _data_src{ std::nullopt };
  std::optional<TransactionAbort> _transaction_abort{ std::nullopt };
  std::optional<Weight> _weight{ std::nullopt };
  std::optional<std::vector<Branch>> _branches{ std::nullopt };
  std::optional<std::uint64_t> _user_registers_abi{ std::nullopt };
  std::optional<std::vector<std::uint64_t>> _user_registers{ std::nullopt };
  std::optional<std::vector<std::uint64_t>> _kernel_registers{ std::nullopt };
  std::optional<std::uint64_t> _kernel_registers_abi{ std::nullopt };
  std::optional<std::vector<std::uintptr_t>> _callchain{ std::nullopt };
  std::optional<std::uint64_t> _cgroup_id{ std::nullopt };
  std::optional<std::uint64_t> _data_page_size{ std::nullopt };
  std::optional<std::uint64_t> _code_page_size{ std::nullopt };
  std::optional<std::uint64_t> _count_loss{ std::nullopt };
  std::optional<CGroup> _cgroup{ std::nullopt };
  std::optional<ContextSwitch> _context_switch{ std::nullopt };
  std::optional<Throttle> _throttle{ std::nullopt };
  bool _is_exact_ip{ false };
};
}