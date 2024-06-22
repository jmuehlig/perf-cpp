#pragma once

#include "counter.h"
#include <cstdint>
#include <linux/perf_event.h>
#include <optional>

namespace perf {
class DataSource
{
public:
  DataSource(const std::uint64_t data_source) noexcept
    : _data_source(data_source)
  {
  }
  ~DataSource() noexcept = default;

  [[nodiscard]] bool is_load() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_op & PERF_MEM_OP_LOAD);
  }
  [[nodiscard]] bool is_store() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_op & PERF_MEM_OP_STORE);
  }
  [[nodiscard]] bool is_prefetch() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_op & PERF_MEM_OP_PFETCH);
  }
  [[nodiscard]] bool is_exec() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_op & PERF_MEM_OP_EXEC);
  }
  [[nodiscard]] bool is_na() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_op & PERF_MEM_OP_NA);
  }

  [[nodiscard]] bool is_mem_hit() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl & PERF_MEM_LVL_HIT);
  }
  [[nodiscard]] bool is_mem_miss() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl & PERF_MEM_LVL_MISS);
  }
  [[nodiscard]] bool is_mem_l1() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl & PERF_MEM_LVL_L1);
  }
  [[nodiscard]] bool is_mem_lfb() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl & PERF_MEM_LVL_LFB);
  }
  [[nodiscard]] bool is_mem_l2() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl & PERF_MEM_LVL_L2);
  }
  [[nodiscard]] bool is_mem_l3() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl & PERF_MEM_LVL_L3);
  }
  [[nodiscard]] bool is_mem_local_ram() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl & PERF_MEM_LVL_LOC_RAM);
  }
  [[nodiscard]] bool is_mem_remote_ram1() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl &
                             PERF_MEM_LVL_REM_RAM1);
  }
  [[nodiscard]] bool is_mem_remote_ram2() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl &
                             PERF_MEM_LVL_REM_RAM2);
  }
  [[nodiscard]] bool is_mem_remote_cce1() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl &
                             PERF_MEM_LVL_REM_CCE1);
  }
  [[nodiscard]] bool is_mem_remote_cce2() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl &
                             PERF_MEM_LVL_REM_CCE2);
  }

  [[nodiscard]] bool is_tlb_hit() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_dtlb & PERF_MEM_TLB_HIT);
  }
  [[nodiscard]] bool is_tlb_miss() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_dtlb & PERF_MEM_TLB_MISS);
  }
  [[nodiscard]] bool is_tlb_l1() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_dtlb & PERF_MEM_TLB_L1);
  }
  [[nodiscard]] bool is_tlb_l2() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_dtlb & PERF_MEM_TLB_L2);
  }
  [[nodiscard]] bool is_tlb_walk() const noexcept
  {
    return static_cast<bool>(reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_dtlb & PERF_MEM_TLB_WK);
  }

private:
  std::uint64_t _data_source;
};

class Branch
{
public:
  Branch(const std::uintptr_t instruction_pointer_from,
         const std::uintptr_t instruction_pointer_to,
         const bool is_mispredicted,
         const bool is_predicted,
         const bool is_in_transaction,
         const bool is_transaction_abort,
         const std::uint16_t cycles)
    : _instruction_pointer_from(instruction_pointer_from)
    , _instruction_pointer_to(instruction_pointer_to)
    , _is_mispredicted(is_mispredicted)
    , _is_predicted(is_predicted)
    , _is_in_transaction(is_in_transaction)
    , _is_transaction_abort(is_transaction_abort)
    , _cycles(cycles)
  {
    // Constructor body (if needed for further initialization)
  }

  [[nodiscard]] std::uintptr_t instruction_pointer_from() const noexcept { return _instruction_pointer_from; }
  [[nodiscard]] std::uintptr_t instruction_pointer_to() const noexcept { return _instruction_pointer_to; }
  [[nodiscard]] bool is_mispredicted() const noexcept { return _is_mispredicted; }
  [[nodiscard]] bool is_predicted() const noexcept { return _is_predicted; }
  [[nodiscard]] bool is_in_transaction() const noexcept { return _is_in_transaction; }
  [[nodiscard]] bool is_transaction_abort() const noexcept { return _is_transaction_abort; }
  [[nodiscard]] std::uint16_t cycles() const noexcept { return _cycles; }

private:
  std::uintptr_t _instruction_pointer_from;
  std::uintptr_t _instruction_pointer_to;
  bool _is_mispredicted;
  bool _is_predicted;
  bool _is_in_transaction;
  bool _is_transaction_abort;
  std::uint16_t _cycles;
};

class Weight
{
public:
  Weight(const std::uint32_t latency, const std::uint16_t var2_w, const std::uint16_t var3_w) noexcept
    : _latency(latency)
    , _var2(var2_w)
    , _var3(var3_w)
  {
  }

  explicit Weight(const std::uint32_t latency) noexcept
    : _latency(latency)
  {
  }

  ~Weight() noexcept = default;

  [[nodiscard]] std::uint32_t latency() const noexcept { return _latency; }
  [[nodiscard]] std::uint32_t var2() const noexcept { return _var2; }
  [[nodiscard]] std::uint32_t var3() const noexcept { return _var3; }

private:
  std::uint32_t _latency;
  std::uint16_t _var2{ 0U };
  std::uint16_t _var3{ 0U };
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

  Sample(Mode mode) noexcept
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
  void data_page_size(const std::uint64_t size) noexcept { _data_page_size = size; }
  void code_page_size(const std::uint64_t size) noexcept { _code_page_size = size; }

  [[nodiscard]] Mode mode() const noexcept { return _mode; }
  [[nodiscard]] std::optional<std::uint64_t> sample_id() const noexcept { return _sample_id; }
  [[nodiscard]] std::optional<std::uintptr_t> instruction_pointer() const noexcept { return _instruction_pointer; }
  [[nodiscard]] std::optional<std::uint32_t> process_id() const noexcept { return _process_id; }
  [[nodiscard]] std::optional<std::uint32_t> thread_id() const noexcept { return _thread_id; }
  [[nodiscard]] std::optional<std::uint64_t> time() const noexcept { return _time; }
  [[nodiscard]] std::optional<std::uintptr_t> logical_memory_address() const noexcept
  {
    return _logical_memory_address;
  }
  [[nodiscard]] std::optional<std::uintptr_t> physical_memory_address() const noexcept
  {
    return _physical_memory_address;
  }
  [[nodiscard]] std::optional<std::uint64_t> id() const noexcept { return _id; }
  [[nodiscard]] std::optional<std::uint32_t> cpu_id() const noexcept { return _cpu_id; }
  [[nodiscard]] std::optional<std::uint64_t> period() const noexcept { return _period; }
  [[nodiscard]] const std::optional<CounterResult>& counter_result() const noexcept { return _counter_result; }
  [[nodiscard]] std::optional<DataSource> data_src() const noexcept { return _data_src; }
  [[nodiscard]] std::optional<Weight> weight() const noexcept { return _weight; }
  [[nodiscard]] const std::optional<std::vector<Branch>>& branches() const noexcept { return _branches; }
  [[nodiscard]] std::optional<std::vector<Branch>>& branches() noexcept { return _branches; }
  [[nodiscard]] std::optional<std::uint64_t> user_registers_abi() const noexcept { return _user_registers_abi; }
  [[nodiscard]] const std::optional<std::vector<std::uint64_t>>& user_registers() const noexcept
  {
    return _user_registers;
  }
  [[nodiscard]] std::optional<std::vector<std::uint64_t>>& user_registers() noexcept { return _user_registers; }
  [[nodiscard]] std::optional<std::uint64_t> kernel_registers_abi() const noexcept { return _kernel_registers_abi; }
  [[nodiscard]] const std::optional<std::vector<std::uint64_t>>& kernel_registers() const noexcept
  {
    return _kernel_registers;
  }
  [[nodiscard]] std::optional<std::vector<std::uint64_t>>& kernel_registers() noexcept { return _kernel_registers; }
  [[nodiscard]] const std::optional<std::vector<std::uintptr_t>>& callchain() const noexcept { return _callchain; }
  [[nodiscard]] std::optional<std::vector<std::uintptr_t>>& callchain() noexcept { return _callchain; }
  [[nodiscard]] std::optional<std::uint64_t> data_page_size() const noexcept { return _data_page_size; }
  [[nodiscard]] std::optional<std::uint64_t> code_page_size() const noexcept { return _code_page_size; }

private:
  Mode _mode;
  std::optional<std::uint64_t> _sample_id{ std::nullopt };
  std::optional<std::uintptr_t> _instruction_pointer{ std::nullopt };
  std::optional<std::uint32_t> _process_id{ std::nullopt };
  std::optional<std::uint32_t> _thread_id{ std::nullopt };
  std::optional<std::uint64_t> _time{ std::nullopt };
  std::optional<std::uintptr_t> _logical_memory_address{ std::nullopt };
  std::optional<std::uintptr_t> _physical_memory_address{ std::nullopt };
  std::optional<std::uint64_t> _id{ std::nullopt };
  std::optional<std::uint32_t> _cpu_id{ std::nullopt };
  std::optional<std::uint64_t> _period{ std::nullopt };
  std::optional<CounterResult> _counter_result{ std::nullopt };
  std::optional<DataSource> _data_src{ std::nullopt };
  std::optional<Weight> _weight{ std::nullopt };
  std::optional<std::vector<Branch>> _branches{ std::nullopt };
  std::optional<std::uint64_t> _user_registers_abi{ std::nullopt };
  std::optional<std::vector<std::uint64_t>> _user_registers{ std::nullopt };
  std::optional<std::vector<std::uint64_t>> _kernel_registers{ std::nullopt };
  std::optional<std::uint64_t> _kernel_registers_abi{ std::nullopt };
  std::optional<std::vector<std::uintptr_t>> _callchain{ std::nullopt };
  std::optional<std::uint64_t> _data_page_size{ std::nullopt };
  std::optional<std::uint64_t> _code_page_size{ std::nullopt };
};
}