#pragma once

#include "branch.h"
#include "cgroup.h"
#include "context_switch.h"
#include "counter_result.h"
#include "data_access.h"
#include "instruction_execution.h"
#include "metadata.h"
#include "registers.h"
#include "throttle.h"
#include "latency.h"
#include "weight.h"
#include <cstdint>
#include <linux/perf_event.h>
#include <optional>

namespace perf {

class Sample
{
public:
  /**
   * Set the list of events and values for the sample.
   *
   * @param counter_result Counter values.
   */
  void counter_result(CounterResult&& counter_result) noexcept { _counter_result.emplace(std::move(counter_result)); }

  /**
   * Set the branch stack.
   *
   * @param branch_stack Branch stack to set.
   */
  void branch_stack(std::vector<Branch>&& branch_stack) noexcept { _branch_stack.emplace(std::move(branch_stack)); }

  /**
   * Set the user stack.
   *
   * @param user_stack User stack to set.
   */
  void user_stack(std::vector<std::byte>&& user_stack) noexcept { _user_stack.emplace(std::move(user_stack)); }

  /**
   * Set the user registers.
   *
   * @param user_registers User registers to set.
   */
  void user_registers(RegisterValues&& user_registers) noexcept { _user_registers.emplace(std::move(user_registers)); }

  /**
   * Set the kernel registers.
   *
   * @param kernel_registers User registers to set.
   */
  void kernel_registers(RegisterValues&& kernel_registers) noexcept
  {
    _kernel_registers.emplace(std::move(kernel_registers));
  }

  /**
   * Set the cgroup ID.
   *
   * @param cgroup_id CGroup ID to set.
   */
  void cgroup_id(const std::uint64_t cgroup_id) noexcept { _cgroup_id.emplace(cgroup_id); }

  /**
   * Set the cgroup details.
   *
   * @param cgroup CGroup details to set.
   */
  void cgroup(CGroup&& cgroup) noexcept { _cgroup.emplace(std::move(cgroup)); }

  /**
   * Set the context switch details.
   *
   * @param context_switch Context switch details to set.
   */
  void context_switch(ContextSwitch&& context_switch) noexcept { _context_switch.emplace(context_switch); }

  /**
   * Set the throttle details.
   *
   * @param throttle Throttle details to set.
   */
  void throttle(Throttle&& throttle) noexcept { _throttle.emplace(throttle); }

  /**
   * Set the raw data.
   *
   * @param raw Raw data to set.
   */
  void raw(std::vector<std::byte>&& raw) noexcept { _raw.emplace(std::move(raw)); }

  /**
   * Set the count loss.
   *
   * @param count_loss Count loss to set.
   */
  void count_loss(const std::uint64_t count_loss) noexcept { _count_loss.emplace(count_loss); }

  /**
   * @return Metadata of the sample.
   */
  [[nodiscard]] const Metadata& metadata() const noexcept { return _metadata; }

  /**
   * @return Metadata of the sample.
   */
  [[nodiscard]] Metadata& metadata() noexcept { return _metadata; }

  /**
   * @return Instruction execution details.
   */
  [[nodiscard]] const InstructionExecution& instruction_execution() const noexcept { return _instruction_execution; }

  /**
   * @return Instruction execution details.
   */
  [[nodiscard]] InstructionExecution& instruction_execution() noexcept { return _instruction_execution; }

  /**
   * @return Data access details.
   */
  [[nodiscard]] const DataAccess& data_access() const noexcept { return _data_access; }

  /**
   * @return Data access details.
   */
  [[nodiscard]] DataAccess& data_access() noexcept { return _data_access; }

  /**
   * @return Counter result.
   */
  [[nodiscard]] const std::optional<CounterResult>& counter_result() const noexcept { return _counter_result; }

  /**
   * @return Optional branch stack.
   */
  [[nodiscard]] const std::optional<std::vector<Branch>>& branch_stack() const noexcept { return _branch_stack; }

  /**
   * @return Optional user stack.
   */
  [[nodiscard]] const std::optional<std::vector<std::byte>>& user_stack() const noexcept { return _user_stack; }

  /**
   * @return User-level registers, if sampled.
   */
  [[nodiscard]] const std::optional<RegisterValues>& user_registers() const noexcept { return _user_registers; }

  /**
   * @return Kernel-level registers, if sampled.
   */
  [[nodiscard]] const std::optional<RegisterValues>& kernel_registers() const noexcept { return _kernel_registers; }

  /**
   * @return Optional cgroup ID.
   */
  [[nodiscard]] std::optional<std::uint64_t> cgroup_id() const noexcept { return _cgroup_id; }

  /**
   * @return Optional cgroup details.
   */
  [[nodiscard]] const std::optional<CGroup>& cgroup() const noexcept { return _cgroup; }

  /**
   * @return Optional context switch details.
   */
  [[nodiscard]] const std::optional<ContextSwitch>& context_switch() const noexcept { return _context_switch; }

  /**
   * @return Optional throttle details.
   */
  [[nodiscard]] const std::optional<Throttle>& throttle() const noexcept { return _throttle; }

  /**
   * @return Optional raw data.
   */
  [[nodiscard]] const std::optional<std::vector<std::byte>>& raw() const noexcept { return _raw; }

  /**
   * @return Optional count loss.
   */
  [[nodiscard]] std::optional<std::uint64_t> count_loss() const noexcept { return _count_loss; }

  /*
   * Returns the mode in which the sample was taken (e.g., Kernel, User, Hypervisor).
   *
   * @return The sample mode.
   */
  [[deprecated("Will be removed in v0.12. Use metadata().mode() instead.")]] [[nodiscard]] Metadata::Mode mode() const noexcept { return _metadata.mode().value_or(Metadata::Mode::Unknown); }

  /*
   * Retrieves the unique identifier for the sample.
   *
   * @return An optional containing the sample ID if available.
   */
  [[deprecated("Will be removed in v0.12. Use metadata().sample_id() instead.")]] [[nodiscard]] std::optional<std::uint64_t> sample_id() const noexcept { return _metadata.sample_id(); }

  /*
   * Retrieves the instruction pointer at the time the sample was recorded.
   *
   * @return An optional containing the instruction pointer address if available.
   */
  [[deprecated("Will be removed in v0.12. Use instruction_execution().logical_instruction_pointer() instead.")]] [[nodiscard]] std::optional<std::uintptr_t> instruction_pointer() const noexcept { return _instruction_execution.logical_instruction_pointer(); }

  /*
   * Retrieves the process ID associated with the sample.
   *
   * @return An optional containing the process ID if available.
   */
  [[deprecated("Will be removed in v0.12. Use metadata().process_id() instead.")]] [[nodiscard]] std::optional<std::uint32_t> process_id() const noexcept { return _metadata.process_id(); }

  /*
   * Retrieves the thread ID associated with the sample.
   *
   * @return An optional containing the thread ID if available.
   */
  [[deprecated("Will be removed in v0.12. Use metadata().thread_id() instead.")]] [[nodiscard]] std::optional<std::uint32_t> thread_id() const noexcept { return _metadata.thread_id(); }

  /*
   * Retrieves the timestamp when the sample was taken.
   *
   * @return An optional containing the timestamp if available.
   */
  [[deprecated("Will be removed in v0.12. Use metadata().timestamp() instead.")]] [[nodiscard]] std::optional<std::uint64_t> time() const noexcept { return _metadata.timestamp(); }

  /*
   * Retrieves the stream id.
   *
   * @return An optional containing the stream id if available.
   */
  [[deprecated("Will be removed in v0.12. Use metadata().stream_id() instead.")]] [[nodiscard]] std::optional<std::uint64_t> stream_id() const noexcept { return _metadata.stream_id(); }

  /*
   * Retrieves the logical (virtual) memory address relevant to the sample.
   *
   * @return An optional containing the logical memory address if available.
   */
  [[deprecated("Will be removed in v0.12. Use data_access().logical_memory_address() instead.")]] [[nodiscard]] std::optional<std::uintptr_t> logical_memory_address() const noexcept
  {
    return _data_access.logical_memory_address();
  }

  /*
   * Retrieves the physical memory address relevant to the sample.
   *
   * @return An optional containing the physical memory address if available.
   */
  [[deprecated("Will be removed in v0.12. Use data_access().physical_memory_address() instead.")]] [[nodiscard]] std::optional<std::uintptr_t> physical_memory_address() const noexcept
  {
    return _data_access.physical_memory_address();
  }

  /*
   * Retrieves the unique ID of the perf_event that generated the sample.
   *
   * @return An optional containing the perf_event ID if available.
   */
  [[deprecated("Will be removed in v0.12. Use metadata().sample_id() instead.")]] [[nodiscard]] std::optional<std::uint64_t> id() const noexcept { return _metadata.sample_id(); }

  /*
   * Retrieves the CPU ID where the sample was collected.
   *
   * @return An optional containing the CPU ID if available.
   */
  [[deprecated("Will be removed in v0.12. Use metadata().cpu_id() instead.")]] [[nodiscard]] std::optional<std::uint32_t> cpu_id() const noexcept { return _metadata.cpu_id(); }

  /*
   * Retrieves the period value indicating the number of events that have occurred.
   *
   * @return An optional containing the period value if available.
   */
  [[deprecated("Will be removed in v0.12. Use metadata().period() instead.")]] [[nodiscard]] std::optional<std::uint64_t> period() const noexcept { return _metadata.period(); }

  /*
   * TODO
   * 
   * Retrieves the data source information of the sample.
   *
   * @return An optional containing the data source if available.
   */
  //[[nodiscard]] std::optional<DataSource> data_src() const noexcept { return _data_src; }

  /*
   * Retrieves the transaction abort of the sample.
   *
   * @return An optional containing the transaction abort if available.
   */
  [[deprecated("Will be removed in v0.12. Use instruction_execution().hardware_transaction_abort() instead.")]] [[nodiscard]] std::optional<InstructionExecution::HardwareTransactionAbort> transaction_abort() const noexcept { return _instruction_execution.hardware_transaction_abort(); }

  /*
   * Retrieves the original weight value representing the cost or impact of the sample.
   *
   * @return An optional containing the weight if available.
   */
  [[deprecated("Will be removed in v0.12. Use instruction_execution().latency() and data_access().latency() instead.")]] [[nodiscard]] std::optional<Weight> weight() const noexcept {
    auto cache_latency = _data_access.latency().data_access();
    if (!cache_latency.has_value()) {
      cache_latency = _data_access.latency().cache_miss();
    }


    auto instruction_retirement_latency = _instruction_execution.latency().instruction_retirement();
    if (!instruction_retirement_latency.has_value()) {
      instruction_retirement_latency = _instruction_execution.latency().uop_tag_to_retirement();
    }


    if (!instruction_retirement_latency.has_value() && !cache_latency.has_value()) {
      return std::nullopt;
    }

    return Weight{cache_latency.value_or(0U), instruction_retirement_latency.value_or(0U), 0U};
  }

  /*
   * Retrieves the latency information, derived from the weight provided by the perf subsystem.
   *
   * @return An optional containing the latency if available.
   */
  [[deprecated("Will be removed in v0.12. Use instruction_execution().latency() and data_access().latency() instead.")]] [[nodiscard]] std::optional<Latency> latency() const noexcept {
    auto instruction_retirement_latency = _instruction_execution.latency().instruction_retirement();
    if (!instruction_retirement_latency.has_value()) {
      instruction_retirement_latency = _instruction_execution.latency().uop_tag_to_retirement();
    }

    auto cache_latency = _data_access.latency().data_access();
    if (!cache_latency.has_value()) {
      cache_latency = _data_access.latency().cache_miss();
    }

    if (!instruction_retirement_latency.has_value() && !cache_latency.has_value()) {
      return std::nullopt;
    }

    return Latency{instruction_retirement_latency.value_or(0U), cache_latency.value_or(0U)};
  }

  /*
   * Retrieves the branches recorded in the sample.
   *
   * @return An optional vector of branches if available.
   */
  [[deprecated("Will be removed in v0.12. Use branch_stack() instead.")]] [[nodiscard]] const std::optional<std::vector<Branch>>& branches() const noexcept { return branch_stack(); }

  /*
   * Retrieves the branches recorded in the sample (modifiable).
   *
   * @return An optional vector of branches if available.
   */
  [[deprecated("Will be removed in v0.12. Use branch_stack() instead.")]] [[nodiscard]] std::optional<std::vector<Branch>>& branches() noexcept { return _branch_stack; }

  /*
   * Retrieves the ABI of the user-space registers.
   *
   * @return An optional containing the user registers ABI if available.
   */
  [[deprecated("Will be removed in v0.12. Use user_registers().abi() instead.")]] [[nodiscard]] std::optional<ABI> user_registers_abi() const noexcept { return _user_registers.has_value() ? std::make_optional(_user_registers.value().abi()) : std::nullopt; }

  /*
   * Retrieves the ABI of the kernel-space registers.
   *
   * @return An optional containing the kernel registers ABI if available.
   */
  [[deprecated("Will be removed in v0.12. Use kernel_registers().abi() instead.")]] [[nodiscard]] std::optional<ABI> kernel_registers_abi() const noexcept { return _kernel_registers.has_value() ? std::make_optional(_kernel_registers.value().abi()) : std::nullopt;; }

  /*
   * Retrieves the call chain (stack backtrace) captured in the sample (modifiable).
   *
   * @return An optional vector of instruction pointers if available.
   */
  [[deprecated("Will be removed in v0.12. Use instruction_execution().callchain() instead.")]] [[nodiscard]] const std::optional<std::vector<std::uintptr_t>>& callchain() noexcept { return _instruction_execution.callchain(); }

  /*
   * Retrieves the data page size at the time of the sample.
   *
   * @return An optional containing the data page size if available.
   */
  [[deprecated("Will be removed in v0.12. Use data_access().page_size() instead.")]] [[nodiscard]] std::optional<std::uint64_t> data_page_size() const noexcept { return _data_access.page_size(); }

  /*
   * Retrieves the code page size at the time of the sample.
   *
   * @return An optional containing the code page size if available.
   */
  [[deprecated("Will be removed in v0.12. Use instruction_execution().page_size() instead.")]] [[nodiscard]] std::optional<std::uint64_t> code_page_size() const noexcept { return _instruction_execution.page_size(); }

  /*
   * Indicates whether the instruction pointer in the sample is exact.
   *
   * @return True if the instruction pointer is exact; otherwise, false.
   */
  [[deprecated("Will be removed in v0.12. Use instruction_execution().is_instruction_pointer_exact() instead.")]] [[nodiscard]] bool is_exact_ip() const noexcept { return _instruction_execution.is_instruction_pointer_exact(); }

private:
  Metadata _metadata;
  InstructionExecution _instruction_execution;
  DataAccess _data_access;
  std::optional<CounterResult> _counter_result{ std::nullopt };
  std::optional<std::vector<Branch>> _branch_stack{ std::nullopt };
  std::optional<std::vector<std::byte>> _user_stack{ std::nullopt };
  std::optional<RegisterValues> _user_registers{ std::nullopt };
  std::optional<RegisterValues> _kernel_registers{ std::nullopt };
  std::optional<std::uint64_t> _cgroup_id{ std::nullopt };
  std::optional<CGroup> _cgroup{ std::nullopt };
  std::optional<ContextSwitch> _context_switch{ std::nullopt };
  std::optional<Throttle> _throttle{ std::nullopt };
  std::optional<std::vector<std::byte>> _raw{ std::nullopt };
  std::optional<std::uint64_t> _count_loss{ std::nullopt };
};
}