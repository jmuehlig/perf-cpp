#pragma once

#include "counter.h"
#include <cstdint>
#include <linux/perf_event.h>
#include <optional>

namespace perf {
class DataSource
{
public:
  explicit DataSource(const std::uint64_t data_source) noexcept
    : _data_source(data_source)
  {
  }
  ~DataSource() noexcept = default;

  /**
   * @return True, if the memory operation is a load.
   */
  [[nodiscard]] bool is_load() const noexcept { return static_cast<bool>(op() & PERF_MEM_OP_LOAD); }

  /**
   * @return True, if the memory operation is a store.
   */
  [[nodiscard]] bool is_store() const noexcept { return static_cast<bool>(op() & PERF_MEM_OP_STORE); }

  /**
   * @return True, if the memory operation is a prefetch.
   */
  [[nodiscard]] bool is_prefetch() const noexcept { return static_cast<bool>(op() & PERF_MEM_OP_PFETCH); }

  /**
   * @return True, if the memory operation is execute.
   */
  [[nodiscard]] bool is_exec() const noexcept { return static_cast<bool>(op() & PERF_MEM_OP_EXEC); }

  /**
   * @return True, if the memory operation is Not Available.
   */
  [[nodiscard]] bool is_na() const noexcept { return static_cast<bool>(op() & PERF_MEM_OP_NA); }

  /**
   * @return True, if the memory operation is a hit.
   */
  [[nodiscard]] bool is_mem_hit() const noexcept { return static_cast<bool>(lvl() & PERF_MEM_LVL_HIT); }

  /**
   * @return True, if the memory operation is a miss.
   */
  [[nodiscard]] bool is_mem_miss() const noexcept { return static_cast<bool>(lvl() & PERF_MEM_LVL_MISS); }

  /**
   * @return True, if the memory address was found in the L1 cache.
   */
  [[nodiscard]] bool is_mem_l1() const noexcept { return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_L1); }

  /**
   * @return True, if the memory address was found in the Line Fill Buffer (or Miss Address Buffer on AMD).
   */
  [[nodiscard]] bool is_mem_lfb() const noexcept { return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_LFB); }

  /**
   * @return True, if the memory address was found in the L2 cache.
   */
  [[nodiscard]] bool is_mem_l2() const noexcept { return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_L2); }

  /**
   * @return True, if the memory address was found in the L3 cache.
   */
  [[nodiscard]] bool is_mem_l3() const noexcept { return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_L3); }

  /**
   * @return True, if the memory address was found in the L4 cache.
   */
  [[nodiscard]] bool is_mem_l4() const noexcept { return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_L4); }

  /**
   * @return True, if the memory address was found in the RAM.
   */
  [[nodiscard]] bool is_mem_ram() const noexcept { return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_RAM); }

  /**
   * @return True, if the memory address was found in the local RAM.
   */
  [[nodiscard]] bool is_mem_local_ram() const noexcept
  {
    return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_RAM) && static_cast<bool>(remote() != PERF_MEM_REMOTE_REMOTE);
  }

  /**
   * @return True, if the memory address was found in any remote RAM.
   */
  [[nodiscard]] bool is_mem_remote_ram() const noexcept
  {
    return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_RAM) && static_cast<bool>(remote() == PERF_MEM_REMOTE_REMOTE);
  }
#if defined(PERF_MEM_HOPS_0) && defined(PERF_MEM_HOPS_1) && (PERF_MEM_HOPS_2) && (PERF_MEM_HOPS_3)
  /**
   * @return True, if the memory address was found in the local RAM.
   */
  [[nodiscard]] bool is_mem_hops0() const noexcept { return static_cast<bool>(hops() == PERF_MEM_HOPS_0); }

  /**
   * @return True, if the memory address was found with one hop distance (same node).
   */
  [[nodiscard]] bool is_mem_hops1() const noexcept { return static_cast<bool>(hops() == PERF_MEM_HOPS_1); }

  /**
   * @return True, if the memory address was found with two hops distance (remote socket, same board).
   */
  [[nodiscard]] bool is_mem_hops2() const noexcept { return static_cast<bool>(hops() == PERF_MEM_HOPS_2); }

  /**
   * @return True, if the memory address was found with three hops distance (remote board).
   */
  [[nodiscard]] bool is_mem_hops3() const noexcept { return static_cast<bool>(hops() == PERF_MEM_HOPS_3); }
#else
  /**
   * @return True, if the memory address was found in the local RAM.
   */
  [[nodiscard]] bool is_mem_hops0() const noexcept { return is_mem_local_ram(); }

  /**
   * @return True, if the memory address was found with one hop distance (same node).
   */
  [[nodiscard]] bool is_mem_hops1() const noexcept { return false; }

  /**
   * @return True, if the memory address was found with two hops distance (remote socket, same board).
   */
  [[nodiscard]] bool is_mem_hops2() const noexcept { return false; }

  /**
   * @return True, if the memory address was found with three hops distance (remote board).
   */
  [[nodiscard]] bool is_mem_hops3() const noexcept { return false; }
#endif

  /**
   * @return True, if the memory address was found in a remote RAM with one hop distance.
   */
  [[nodiscard]] bool is_mem_remote_ram1() const noexcept { return is_mem_remote_ram() && is_mem_hops1(); }

  /**
   * @return True, if the memory address was found in a remote RAM with two hops distance.
   */
  [[nodiscard]] bool is_mem_remote_ram2() const noexcept { return is_mem_remote_ram() && is_mem_hops2(); }

  /**
   * @return True, if the memory address was found in a remote RAM with three hops distance.
   */
  [[nodiscard]] bool is_mem_remote_ram3() const noexcept { return is_mem_remote_ram() && is_mem_hops3(); }

  /**
   * @return True, if the memory address was found in a remote cache with one hop distance.
   */
  [[nodiscard]] bool is_mem_remote_cce1() const noexcept { return static_cast<bool>(lvl() & PERF_MEM_LVL_REM_CCE1); }

  /**
   * @return True, if the memory address was found in a remote cache with two hops distance.
   */
  [[nodiscard]] bool is_mem_remote_cce2() const noexcept { return static_cast<bool>(lvl() & PERF_MEM_LVL_REM_CCE2); }

#ifdef PERF_MEM_LVLNUM_PMEM
  /**
   * @return True, if the memory address is stored in a PMEM module.
   */
  [[nodiscard]] bool is_pmem() const noexcept { return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_PMEM); }
#else
  /**
   * @return True, if the memory address is stored in a PMEM module.
   */
  [[nodiscard]] bool is_pmem() const noexcept { return false; }
#endif

#ifdef PERF_MEM_LVLNUM_CXL
  /**
   * @return True, if the memory address is transferred via Compute Express Link.
   */
  [[nodiscard]] bool is_cxl() const noexcept { return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_CXL); }
#else
  /**
   * @return True, if the memory address is transferred via Compute Express Link.
   */
  [[nodiscard]] bool is_cxl() const noexcept { return false; }
#endif

#ifdef PERF_MEM_LVLNUM_IO
  /**
   * @return True, if the memory address is I/O.
   */
  [[nodiscard]] bool is_io() const noexcept { return static_cast<bool>(lvl_num() == PERF_MEM_LVLNUM_IO); }
#else
  /**
   * @return True, if the memory address is I/O.
   */
  [[nodiscard]] bool is_io() const noexcept { return false; }
#endif

  /**
   * @return True, if the memory address was a TLB hit.
   */
  [[nodiscard]] bool is_tlb_hit() const noexcept { return static_cast<bool>(tlb() & PERF_MEM_TLB_HIT); }

  /**
   * @return True, if the memory address was a TLB miss.
   */
  [[nodiscard]] bool is_tlb_miss() const noexcept { return static_cast<bool>(tlb() & PERF_MEM_TLB_MISS); }

  /**
   * @return True, if the access can be associated with the dTLB.
   */
  [[nodiscard]] bool is_tlb_l1() const noexcept { return static_cast<bool>(tlb() & PERF_MEM_TLB_L1); }

  /**
   * @return True, if the access can be associated with the STLB.
   */
  [[nodiscard]] bool is_tlb_l2() const noexcept { return static_cast<bool>(tlb() & PERF_MEM_TLB_L2); }

  /**
   * @return True, if the access can be associated with the hardware walker.
   */
  [[nodiscard]] bool is_tlb_walk() const noexcept { return static_cast<bool>(tlb() & PERF_MEM_TLB_WK); }

  /**
   * @return True, If the address was accessed via lock instruction.
   */
  [[nodiscard]] bool is_locked() const noexcept { return static_cast<bool>(lock() & PERF_MEM_LOCK_LOCKED); }

  /**
   * @return True, If the data could not be forwarded.
   */
  [[nodiscard]] bool is_data_blocked() const noexcept { return static_cast<bool>(blk() & PERF_MEM_BLK_DATA); }

  /**
   * @return True in case of an address conflict.
   */
  [[nodiscard]] bool is_address_blocked() const noexcept { return static_cast<bool>(blk() & PERF_MEM_BLK_ADDR); }

  /**
   * @return True, if access was a snoop hit.
   */
  [[nodiscard]] bool is_snoop_hit() const noexcept { return static_cast<bool>(snoop() & PERF_MEM_SNOOP_HIT); }

  /**
   * @return True, if access was a snoop miss.
   */
  [[nodiscard]] bool is_snoop_miss() const noexcept { return static_cast<bool>(snoop() & PERF_MEM_SNOOP_MISS); }

  /**
   * @return True, if access was a snoop hit modified.
   */
  [[nodiscard]] bool is_snoop_hit_modified() const noexcept { return static_cast<bool>(snoop() & PERF_MEM_SNOOP_HITM); }

  /**
   * @return Direct access to the MEM_OP structure of the perf_mem_data_src.
   */
  [[nodiscard]] std::uint64_t op() const noexcept
  {
    return reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_op;
  }

  /**
   * @return Direct access to the MEM_LVL structure of the perf_mem_data_src (note that MEM_LVL is deprecated).
   */
  [[nodiscard]] std::uint64_t lvl() const noexcept
  {
    return reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl;
  }

  /**
   * @return Direct access to the MEM_REMOTE structure of the perf_mem_data_src.
   */
  [[nodiscard]] std::uint64_t remote() const noexcept
  {
    return reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_remote;
  }

  /**
   * @return Direct access to the MEM_LVL_NUM structure of the perf_mem_data_src.
   */
  [[nodiscard]] std::uint64_t lvl_num() const noexcept
  {
    return reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lvl_num;
  }

  /**
   * @return Direct access to the MEM_SNOOP structure of the perf_mem_data_src.
   */
  [[nodiscard]] std::uint64_t snoop() const noexcept
  {
    return reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_snoop;
  }

  /**
   * @return Direct access to the MEM_SNOOPX structure of the perf_mem_data_src.
   */
  [[nodiscard]] std::uint64_t snoopx() const noexcept
  {
    return reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_snoopx;
  }

  /**
   * @return Direct access to the MEM_LOCK structure of the perf_mem_data_src.
   */
  [[nodiscard]] std::uint64_t lock() const noexcept
  {
    return reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_lock;
  }

  /**
   * @return Direct access to the MEM_TLB structure of the perf_mem_data_src.
   */
  [[nodiscard]] std::uint64_t tlb() const noexcept
  {
    return reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_dtlb;
  }

  /**
   * @return Direct access to the MEM_BLK structure of the perf_mem_data_src.
   */
  [[nodiscard]] std::uint64_t blk() const noexcept
  {
    return reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_blk;
  }

  /**
   * @return Direct access to the MEM_HOPS structure of the perf_mem_data_src.
   */
  [[nodiscard]] std::uint64_t hops() const noexcept
  {
#ifdef PERF_MEM_HOPS_0
    return reinterpret_cast<const perf_mem_data_src*>(&_data_source)->mem_hops;
#else
    return 0ULL;
#endif
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

/**
 * Contains information about the "weight" of a memory-based sample, whereas weight mostly refers to the latency of a
 * memory instruction.
 */
class Weight
{
public:
  Weight(const std::uint32_t cache_latency,
         const std::uint16_t instruction_retirement_latency,
         const std::uint16_t var3_w) noexcept
    : _var1(cache_latency)
    , _var2(instruction_retirement_latency)
    , _var3(var3_w)
  {
  }

  explicit Weight(const std::uint32_t cache_latency) noexcept
    : _var1(cache_latency)
  {
  }

  ~Weight() noexcept = default;

  /**
   * @return Latency from cache-access until the memory subsystem returns the data (in CPU core cycles). For stores,
   * this refers to the latency from L1d until data is written to the memory subsystem. The latency does not include TLB
   * lookups.
   */
  [[nodiscard]] std::uint32_t cache_latency() const noexcept { return _var1; }

  /**
   * @return The latency from dispatch until retirement of a load instruction, including TLB lookups.
   */
  [[nodiscard]] std::uint32_t instruction_retirement_latency() const noexcept { return _var2; }

  [[nodiscard]] std::uint32_t latency() const noexcept { return _var1; }
  [[nodiscard]] std::uint32_t var2() const noexcept { return _var2; }
  [[nodiscard]] std::uint32_t var3() const noexcept { return _var3; }

private:
  std::uint32_t _var1;
  std::uint16_t _var2{ 0U };
  std::uint16_t _var3{ 0U };
};

class CGroup
{
public:
  CGroup(const std::uint64_t id, std::string&& path) noexcept
    : _id(id)
    , _path(std::move(path))
  {
  }
  ~CGroup() = default;

  [[nodiscard]] std::uint64_t id() const noexcept { return _id; }

  [[nodiscard]] const std::string& path() const noexcept { return _path; }

private:
  std::uint64_t _id;
  std::string _path;
};

class ContextSwitch
{
public:
  ContextSwitch(const bool is_out, const bool is_preempt) noexcept
    : _is_out(is_out)
    , _is_preempt(is_preempt)
  {
  }
  ContextSwitch(const bool is_out,
                const bool is_preempt,
                const std::uint32_t process_id,
                const std::uint32_t thread_id) noexcept
    : _is_out(is_out)
    , _is_preempt(is_preempt)
    , _process_id(process_id)
    , _thread_id(thread_id)
  {
  }
  ~ContextSwitch() noexcept = default;

  [[nodiscard]] bool is_out() const noexcept { return _is_out; }
  [[nodiscard]] bool is_in() const noexcept { return !_is_out; }
  [[nodiscard]] bool is_preempt() const noexcept { return _is_preempt; }
  [[nodiscard]] std::optional<std::uint32_t> process_id() const noexcept { return _process_id; }
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

  [[nodiscard]] bool is_throttle() const noexcept { return _is_throttle; }
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