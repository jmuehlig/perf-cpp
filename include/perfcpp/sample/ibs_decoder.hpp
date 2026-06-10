#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace perf {
/**
 * Decodes raw IBS fetch sample data captured by the AMD front-end (fetch) PMU.
 *
 * The raw buffer maps the IBS fetch MSRs
 * (IBS_FETCH_CTL, IBS_FETCH_LINADDR, IBS_FETCH_PHYS_ADDR, IBS_FETCH_CTL_EXTD)
 * as delivered by perf_event_open; some fields are not accessible via the standard
 * perf_event_open interface.
 */
class IBSFetchDecoder
{
public:
  explicit IBSFetchDecoder(const std::vector<std::byte>& raw_data) noexcept
  {
    /// The raw buffer is a 4-byte caps word followed by the IBS fetch MSRs; copy only the
    /// registers that are present (trailing MSRs are optional, depending on CPU capabilities).
    if (raw_data.size() > 4U) {
      _raw_size = raw_data.size() - 4U;
      std::memcpy(&_fetch_data, raw_data.data() + 4U, std::min(_raw_size, sizeof(FetchData)));
    }
  }

  IBSFetchDecoder(const IBSFetchDecoder&) = default;
  IBSFetchDecoder(IBSFetchDecoder&&) noexcept = default;
  ~IBSFetchDecoder() noexcept = default;
  IBSFetchDecoder& operator=(const IBSFetchDecoder&) = default;
  IBSFetchDecoder& operator=(IBSFetchDecoder&&) noexcept = default;

  /**
   * Returns true if the raw sample contains all three base fetch MSRs
   * (IBS_FETCH_CTL, IBS_FETCH_LINADDR, IBS_FETCH_PHYS_ADDR).
   *
   * @return True if the base fetch data is fully present.
   */
  [[nodiscard]] bool is_base_data_complete() const noexcept
  {
    return _raw_size >= offsetof(FetchData, _fetch_control_extended);
  }

  /**
   * Returns true if the raw sample contains the extended fetch control MSR
   * (IC_IBS_EXTD_CTL; only written by CPUs with the FetchCtlExtd IBS capability).
   *
   * @return True if the extended fetch control register is present.
   */
  [[nodiscard]] bool has_extended_fetch_control() const noexcept
  {
    return _raw_size >= offsetof(FetchData, _fetch_control_extended) + sizeof(FetchControlExtended);
  }

  /**
   * Returns true if the sampled fetch data is valid.
   *
   * @return True if the fetch sample is valid.
   */
  [[nodiscard]] bool is_valid() const noexcept { return _fetch_data._fetch_control.is_fetch_valid; }

  /**
   * Returns true if the sampled fetch missed in the op cache.
   *
   * @return True if the op cache was missed.
   */
  [[nodiscard]] bool is_op_cache_miss() const noexcept { return _fetch_data._fetch_control.is_fetch_op_cache_miss; }

  /**
   * Returns true if the sampled fetch missed in the L1 instruction cache.
   *
   * @return True if the L1 instruction cache was missed.
   */
  [[nodiscard]] bool is_instruction_cache_miss() const noexcept
  {
    return _fetch_data._fetch_control.is_instruction_cache_miss;
  }

  /**
   * Returns true if the sampled fetch missed in the L2 cache.
   *
   * @return True if the L2 cache was missed.
   */
  [[nodiscard]] bool is_l2_miss() const noexcept { return _fetch_data._fetch_control.is_fetch_l2_miss; }

  /**
   * Returns true if the sampled fetch missed in the L3 cache.
   *
   * @return True if the L3 cache was missed.
   */
  [[nodiscard]] bool is_l3_miss() const noexcept { return _fetch_data._fetch_control.is_fetch_l3_miss; }

  /**
   * Returns true if the fetch missed in the L1 instruction TLB.
   *
   * @return True if the L1 ITLB was missed.
   */
  [[nodiscard]] bool is_l1_tlb_miss() const noexcept { return _fetch_data._fetch_control.is_l1_tlb_miss; }

  /**
   * Returns true if the fetch missed in the L2 instruction TLB.
   *
   * @return True if the L2 ITLB was missed.
   */
  [[nodiscard]] bool is_l2_tlb_miss() const noexcept { return _fetch_data._fetch_control.is_l2_tlb_miss; }

  /**
   * Returns the L1 ITLB page size code for the sampled fetch.
   * Encoding: 0 = 4 KB, 1 = 2 MB, 2 = 1 GB.
   *
   * @return 2-bit page size code.
   */
  [[nodiscard]] std::uint8_t l1_tlb_page_size() const noexcept
  {
    return static_cast<std::uint8_t>(_fetch_data._fetch_control.l1_tlb_page_size);
  }

  /**
   * Returns true if the sampled fetch completed successfully (was sent to the decoder).
   *
   * @return True if the fetch completed.
   */
  [[nodiscard]] bool is_complete() const noexcept { return _fetch_data._fetch_control.is_fetch_complete; }

  /**
   * Returns the number of cycles from fetch dispatch to completion.
   *
   * @return Fetch latency in cycles.
   */
  [[nodiscard]] std::uint16_t latency() const noexcept { return _fetch_data._fetch_control.fetch_latency; }

  /**
   * Returns the number of cycles taken to refill the instruction TLB after a miss.
   * Only meaningful when is_l1_tlb_miss() or is_l2_tlb_miss() is true.
   * Requires CPU support for the extended IBS fetch MSR (Family 17h+).
   * Returns std::nullopt when the extended MSR is not present in the raw buffer.
   *
   * @return ITLB refill latency in cycles, or std::nullopt if not available.
   */
  [[nodiscard]] std::optional<std::uint16_t> itlb_refill_latency() const noexcept
  {
    if (!has_extended_fetch_control()) {
      return std::nullopt;
    }
    return _fetch_data._fetch_control_extended.itlb_refill_latency;
  }

  /**
   * Returns the linear (virtual) address of the sampled instruction fetch.
   *
   * @return Linear instruction address.
   */
  [[nodiscard]] std::uintptr_t linear_instruction_address() const noexcept
  {
    return _fetch_data._linear_instruction_address;
  }

  /**
   * Returns the physical address of the sampled fetch, or zero if the physical address is not valid.
   *
   * @return Physical instruction address, or 0 if unavailable.
   */
  [[nodiscard]] std::uintptr_t physical_instruction_address() const noexcept
  {
    return _fetch_data._physical_instruction_address *
           static_cast<std::uint64_t>(_fetch_data._fetch_control.is_physical_address_valid);
  }

  /**
   * Returns true if the physical instruction address field contains a valid address.
   *
   * @return True if the physical address is valid.
   */
  [[nodiscard]] bool is_physical_instruction_address_valid() const noexcept
  {
    return _fetch_data._fetch_control.is_physical_address_valid;
  }

private:
  /// Mirrors the IBS_FETCH_CTL MSR (0xC0011030).
  struct FetchControl
  {
    /// Programmed fetch count threshold that triggers sampling.
    std::uint64_t fetch_max_count : 16;
    /// Running count of completed fetches since the last sample.
    std::uint64_t fetch_count : 16;
    /// Cycles from fetch dispatch to completion.
    std::uint64_t fetch_latency : 16;
    /// IBS fetch sampling is enabled.
    std::uint64_t is_fetch_enable : 1;
    /// Sampled fetch data is valid.
    std::uint64_t is_fetch_valid : 1;
    /// Fetch completed and was sent to the decoder.
    std::uint64_t is_fetch_complete : 1;
    /// L1 instruction cache miss.
    std::uint64_t is_instruction_cache_miss : 1;
    /// Physical instruction address field is populated.
    std::uint64_t is_physical_address_valid : 1;
    /// L1 ITLB page size code: 0 = 4 KB, 1 = 2 MB, 2 = 1 GB.
    std::uint64_t l1_tlb_page_size : 2;
    /// L1 instruction TLB miss.
    std::uint64_t is_l1_tlb_miss : 1;
    /// L2 instruction TLB miss.
    std::uint64_t is_l2_tlb_miss : 1;
    /// Random tagging is enabled in the fetch counter.
    std::uint64_t is_random_tagging_enabled : 1;
    /// L2 cache miss for the sampled fetch.
    std::uint64_t is_fetch_l2_miss : 1;
    /// Sampling was configured to collect L3 misses only.
    std::uint64_t is_l3_miss_only : 1;
    /// Op cache miss for the sampled fetch.
    std::uint64_t is_fetch_op_cache_miss : 1;
    /// L3 cache miss for the sampled fetch.
    std::uint64_t is_fetch_l3_miss : 1;
    std::uint64_t reserved0 : 2;
  };

  /// Mirrors the IBS_FETCH_CTL_EXTD MSR (0xC0011038, Family 17h+).
  struct FetchControlExtended
  {
    /// Cycles to refill the instruction TLB after a miss.
    std::uint64_t itlb_refill_latency : 16;
    std::uint64_t reserved0 : 48;
  };

  /// Layout of the IBS fetch MSRs as delivered in the perf raw data buffer.
  struct FetchData
  {
    /// IBS_FETCH_CTL MSR.
    FetchControl _fetch_control;
    /// IBS_FETCH_LINADDR MSR: linear (virtual) address of the sampled fetch.
    std::uintptr_t _linear_instruction_address;
    /// IBS_FETCH_PHYS_ADDR MSR: physical address of the sampled fetch.
    std::uintptr_t _physical_instruction_address;
    /// IBS_FETCH_CTL_EXTD MSR: extended fetch control (Family 17h+).
    FetchControlExtended _fetch_control_extended;
  };

  /// The decoders rely on LSB-first bitfield allocation (GCC/Clang on x86-64, the only
  /// platforms with AMD IBS). The asserts lock the MSR image sizes against accidental padding.
  static_assert(sizeof(FetchControl) == 8U);
  static_assert(sizeof(FetchControlExtended) == 8U);
  static_assert(sizeof(FetchData) == 32U);

  /// Value-initialized IBS fetch MSRs; bytes not present in the raw buffer remain zero.
  FetchData _fetch_data{};
  /// Number of payload bytes copied into _fetch_data (raw_data.size() - 4, or 0 if too small).
  std::size_t _raw_size{ 0 };
};

/**
 * Decodes raw IBS op sample data captured by the AMD back-end (execution) PMU.
 *
 * The raw buffer maps the IBS op MSRs
 * (IBS_OP_CTL, IBS_OP_RIP, IBS_OP_DATA, IBS_OP_DATA2, IBS_OP_DATA3,
 * IBS_DC_LINADDR, IBS_DC_PHYSADDR, MSR_AMD64_IBSBRTARGET)
 * as delivered by perf_event_open; some fields are not accessible via the standard
 * perf_event_open interface.
 */
class IBSOpDecoder
{
public:
  explicit IBSOpDecoder(const std::vector<std::byte>& raw_data) noexcept
  {
    /// The raw buffer is a 4-byte caps word followed by the IBS op MSRs; copy only the
    /// registers that are present (trailing MSRs are optional, depending on CPU capabilities).
    if (raw_data.size() > 4U) {
      _raw_size = raw_data.size() - 4U;
      std::memcpy(&_execution_data, raw_data.data() + 4U, std::min(_raw_size, sizeof(ExecutionData)));
    }
  }

  IBSOpDecoder(const IBSOpDecoder&) = default;
  IBSOpDecoder(IBSOpDecoder&&) noexcept = default;
  ~IBSOpDecoder() noexcept = default;
  IBSOpDecoder& operator=(const IBSOpDecoder&) = default;
  IBSOpDecoder& operator=(IBSOpDecoder&&) noexcept = default;

  /**
   * Returns true if the raw sample contains all seven base op MSRs
   * (IBS_OP_CTL through IBS_DC_PHYSADDR).
   *
   * @return True if the base execution data is fully present.
   */
  [[nodiscard]] bool is_base_data_complete() const noexcept
  {
    return _raw_size >= offsetof(ExecutionData, _branch_target_address);
  }

  /**
   * Returns true if the raw sample contains the branch target MSR
   * (MSR_AMD64_IBSBRTARGET; only written by CPUs with the BrnTrgt IBS capability).
   *
   * @return True if the branch target address register is present.
   */
  [[nodiscard]] bool has_branch_target_address() const noexcept
  {
    return _raw_size >= offsetof(ExecutionData, _branch_target_address) + sizeof(std::uintptr_t);
  }

  /**
   * Returns the number of cycles from op completion to retirement.
   *
   * @return Completion-to-retire cycle count.
   */
  [[nodiscard]] std::uint16_t completion_to_retire_latency() const noexcept
  {
    return _execution_data._op_data1.completion_to_retire_count;
  }

  /**
   * Returns the number of cycles from op tag to retirement.
   *
   * @return Tag-to-retire cycle count.
   */
  [[nodiscard]] std::uint16_t tag_to_retire_latency() const noexcept
  {
    return _execution_data._op_data1.tag_to_retire_count;
  }

  /**
   * Returns true if the sampled op is a return instruction.
   *
   * @return True if the op is a return.
   */
  [[nodiscard]] bool is_return_operation() const noexcept { return _execution_data._op_data1.is_return_operation; }

  /**
   * Returns true if the sampled op is a taken branch.
   *
   * @return True if the branch was taken.
   */
  [[nodiscard]] bool is_branch_taken_operation() const noexcept
  {
    return _execution_data._op_data1.is_brn_taken_operation;
  }

  /**
   * Returns true if the sampled op is a mispredicted branch.
   *
   * @return True if the branch was mispredicted.
   */
  [[nodiscard]] bool is_branch_mispredicted_operation() const noexcept
  {
    return _execution_data._op_data1.is_brn_misp_operation;
  }

  /**
   * Returns true if the sampled op is a retired branch.
   *
   * @return True if the branch retired.
   */
  [[nodiscard]] bool is_branch_retired_operation() const noexcept
  {
    return _execution_data._op_data1.is_brn_ret_operation;
  }

  /**
   * Returns true if the sampled op is any branch-class instruction,
   * including taken branches, mispredicted branches, retired branches,
   * fused branches, and return instructions.
   *
   * @return True if the op is a branch-class instruction.
   */
  [[nodiscard]] bool is_branch() const noexcept
  {
    return is_return_operation() || is_branch_taken_operation() || is_branch_mispredicted_operation() ||
           is_branch_retired_operation() || is_branch_fuse();
  }

  /**
   * Returns true if the sampled op is a fused branch.
   *
   * @return True if the branch is fused.
   */
  [[nodiscard]] bool is_branch_fuse() const noexcept { return _execution_data._op_data1.is_brn_fuse; }

  /**
   * Returns true if the sampled op required microcode assistance.
   *
   * @return True if the op is microcode-assisted.
   */
  [[nodiscard]] bool is_microcode() const noexcept { return _execution_data._op_data1.is_microcode; }

  /**
   * Returns true if the data for this load op was sourced from a remote NUMA node.
   *
   * @return True if the data source is a remote node.
   */
  [[nodiscard]] bool is_remote_node() const noexcept { return _execution_data._op_data2.is_remote_node; }

  /**
   * Returns true if the load op hit in a cache.
   *
   * @return True if the access was a cache hit.
   */
  [[nodiscard]] bool is_cache_hit() const noexcept { return _execution_data._op_data2.is_cache_hit; }

  /**
   * Returns true if the sampled op is a load.
   *
   * @return True if the op is a load.
   */
  [[nodiscard]] bool is_load_operation() const noexcept { return _execution_data._op_data3.is_load_operation; }

  /**
   * Returns true if the sampled op is a store.
   *
   * @return True if the op is a store.
   */
  [[nodiscard]] bool is_store_operation() const noexcept { return _execution_data._op_data3.is_store_operation; }

  /**
   * Returns true if the sampled op is a software prefetch instruction.
   *
   * @return True if the op is a software prefetch.
   */
  [[nodiscard]] bool is_software_prefetch() const noexcept { return _execution_data._op_data3.is_software_prefetch; }

  /**
   * Returns true if the data cache access missed in the L1 TLB.
   *
   * @return True if the L1 data TLB was missed.
   */
  [[nodiscard]] bool is_l1_data_tlb_miss() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_l1_tlb_miss;
  }

  /**
   * Returns true if the data cache access missed in the L2 TLB.
   *
   * @return True if the L2 data TLB was missed.
   */
  [[nodiscard]] bool is_l2_data_tlb_miss() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_l2_tlb_miss;
  }

  /**
   * Returns true if the L1 data TLB was hit with a 2 MB page.
   *
   * @return True if the L1 DTLB hit used a 2 MB page.
   */
  [[nodiscard]] bool is_l1_data_tlb_hit_2m() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_l1_tlb_hit_2m;
  }

  /**
   * Returns true if the L1 data TLB was hit with a 1 GB page.
   *
   * @return True if the L1 DTLB hit used a 1 GB page.
   */
  [[nodiscard]] bool is_l1_data_tlb_hit_1g() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_l1_tlb_hit_1g;
  }

  /**
   * Returns true if the L2 data TLB was hit with a 2 MB page.
   *
   * @return True if the L2 DTLB hit used a 2 MB page.
   */
  [[nodiscard]] bool is_l2_data_tlb_hit_2m() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_l2_tlb_hit_2m;
  }

  /**
   * Returns true if the L2 data TLB was hit with a 1 GB page.
   *
   * @return True if the L2 DTLB hit used a 1 GB page.
   */
  [[nodiscard]] bool is_l2_data_tlb_hit_1g() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_l2_tlb_hit_1g;
  }

  /**
   * Returns true if the sampled load or store missed in the L1 data cache.
   *
   * @return True if the L1 data cache was missed.
   */
  [[nodiscard]] bool is_data_cache_miss() const noexcept { return _execution_data._op_data3.is_data_cache_miss; }

  /**
   * Returns true if the sampled memory access crossed a cache line boundary.
   *
   * @return True if the access was misaligned.
   */
  [[nodiscard]] bool is_data_cache_misaligned_access() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_misaligned_access;
  }

  /**
   * Returns true if the sampled memory access targeted write-combining memory.
   *
   * @return True if the access was to write-combining memory.
   */
  [[nodiscard]] bool is_data_cache_write_combine_access() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_write_combine_access;
  }

  /**
   * Returns true if the sampled memory access targeted uncacheable memory.
   *
   * @return True if the access was to uncacheable memory.
   */
  [[nodiscard]] bool is_data_cache_uncachable_access() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_uncachable_access;
  }

  /**
   * Returns true if the sampled memory op was a locked (atomic) operation.
   *
   * @return True if the operation was a locked memory access.
   */
  [[nodiscard]] bool is_data_cache_locked_operation() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_locked_operation;
  }

  /**
   * Returns true if the data cache miss did not allocate a MAB (Miss Address Buffer) entry.
   *
   * @return True if no MAB was allocated for this miss.
   */
  [[nodiscard]] bool is_data_cache_miss_no_mab_allocation() const noexcept
  {
    return _execution_data._op_data3.is_data_cache_miss_no_mab_allocation;
  }

  /**
   * Returns true if the data cache access also missed in the L2 cache.
   *
   * @return True if the access missed in L2.
   */
  [[nodiscard]] bool is_l2_miss() const noexcept { return _execution_data._op_data3.is_l2_miss; }

  /**
   * Returns the encoded width of the memory access.
   * The 4-bit field maps to the actual byte width as defined in the AMD IBS specification.
   *
   * @return Memory access width code.
   */
  [[nodiscard]] std::uint8_t access_mem_width() const noexcept
  {
    return static_cast<std::uint8_t>(_execution_data._op_data3.op_mem_width);
  }

  /**
   * Returns the number of outstanding memory requests at the time of the data cache miss.
   *
   * @return Number of open memory requests.
   */
  [[nodiscard]] std::uint8_t num_open_mem_requests() const noexcept
  {
    return static_cast<std::uint8_t>(_execution_data._op_data3.num_op_data_cache_miss_open_mem_requests);
  }

  /**
   * Returns the number of cycles from load dispatch to data return on a data cache miss.
   *
   * @return Data cache miss latency in cycles.
   */
  [[nodiscard]] std::uint16_t data_cache_miss_latency() const noexcept
  {
    return _execution_data._op_data3.data_cache_miss_latency;
  }

  /**
   * Returns the number of cycles to complete the data TLB refill on a TLB miss.
   * Only meaningful when is_l1_data_tlb_miss() or is_l2_data_tlb_miss() is true.
   *
   * @return TLB refill latency in cycles.
   */
  [[nodiscard]] std::uint16_t tlb_refill_latency() const noexcept
  {
    return _execution_data._op_data3.tlb_refill_latency;
  }

  /**
   * Returns the linear (virtual) address of the sampled instruction.
   *
   * @return Linear instruction address.
   */
  [[nodiscard]] std::uintptr_t linear_instruction_address() const noexcept
  {
    return _execution_data._linear_instruction_address;
  }

  /**
   * Returns the linear (virtual) address of the sampled memory access,
   * or zero if the linear address is not valid.
   *
   * @return Linear data address, or 0 if unavailable.
   */
  [[nodiscard]] std::uintptr_t linear_memory_address() const noexcept
  {
    return _execution_data._linear_memory_address *
           static_cast<std::uint64_t>(_execution_data._op_data3.is_data_cache_linear_address_valid);
  }

  /**
   * Returns the physical address of the sampled memory access,
   * or zero if the physical address is not valid.
   *
   * @return Physical data address, or 0 if unavailable.
   */
  [[nodiscard]] std::uintptr_t physical_memory_address() const noexcept
  {
    return _execution_data._physical_memory_address *
           static_cast<std::uint64_t>(_execution_data._op_data3.is_data_cache_physical_address_valid);
  }

  /**
   * Returns the branch target address for the sampled branch op, or std::nullopt if
   * the MSR_AMD64_IBSBRTARGET register is not present in the raw buffer.
   * Only valid when is_branch() is true. Requires the BrnTrgt IBS capability.
   *
   * @return Branch target address, or std::nullopt if not available.
   */
  [[nodiscard]] std::optional<std::uintptr_t> branch_target_address() const noexcept
  {
    if (!has_branch_target_address()) {
      return std::nullopt;
    }
    return _execution_data._branch_target_address;
  }

private:
  /// Mirrors the IBS_OP_DATA MSR (0xC0011035).
  struct OpData1
  {
    /// Cycles from op completion to retirement.
    std::uint64_t completion_to_retire_count : 16;
    /// Cycles from op tag to retirement.
    std::uint64_t tag_to_retire_count : 16;
    std::uint64_t reserved0 : 2;
    /// Op is a near return instruction.
    std::uint64_t is_return_operation : 1;
    /// Op is a taken branch.
    std::uint64_t is_brn_taken_operation : 1;
    /// Op is a mispredicted branch.
    std::uint64_t is_brn_misp_operation : 1;
    /// Op is a retired branch.
    std::uint64_t is_brn_ret_operation : 1;
    /// RIP of the sampled op is invalid (e.g., micro-op from a fused branch pair).
    std::uint64_t is_rip_invalid : 1;
    /// Op is a fused branch.
    std::uint64_t is_brn_fuse : 1;
    /// Op required microcode assistance.
    std::uint64_t is_microcode : 1;
    std::uint64_t reserved1 : 23;
  };

  /// Mirrors the IBS_OP_DATA2 MSR (0xC0011036).
  struct OpData2
  {
    /// Lower 3 bits of the data source field (DataSrc).
    std::uint64_t data_source_lo : 3;
    std::uint64_t reserved0 : 1;
    /// Data was fetched from a remote NUMA node.
    std::uint64_t is_remote_node : 1;
    /// Cache hit status for the load op.
    std::uint64_t is_cache_hit : 1;
    /// Upper 2 bits of the extended data source field (DataSrcExtension, Family 17h+).
    std::uint64_t data_source_hi : 2;
    std::uint64_t reserved1 : 56;
  };

  /// Mirrors the IBS_OP_DATA3 MSR (0xC0011037).
  struct OpData3
  {
    /// Op is a load.
    std::uint64_t is_load_operation : 1;
    /// Op is a store.
    std::uint64_t is_store_operation : 1;
    /// Data cache access missed in the L1 TLB.
    std::uint64_t is_data_cache_l1_tlb_miss : 1;
    /// Data cache access missed in the L2 TLB.
    std::uint64_t is_data_cache_l2_tlb_miss : 1;
    /// L1 DTLB hit with a 2 MB page.
    std::uint64_t is_data_cache_l1_tlb_hit_2m : 1;
    /// L1 DTLB hit with a 1 GB page.
    std::uint64_t is_data_cache_l1_tlb_hit_1g : 1;
    /// L2 DTLB hit with a 2 MB page.
    std::uint64_t is_data_cache_l2_tlb_hit_2m : 1;
    /// L1 data cache miss.
    std::uint64_t is_data_cache_miss : 1;
    /// Memory access crossed a cache line boundary.
    std::uint64_t is_data_cache_misaligned_access : 1;
    std::uint64_t reserved0 : 4;
    /// Access targeted write-combining memory.
    std::uint64_t is_data_cache_write_combine_access : 1;
    /// Access targeted uncacheable memory.
    std::uint64_t is_data_cache_uncachable_access : 1;
    /// Op is a locked (atomic) memory access.
    std::uint64_t is_data_cache_locked_operation : 1;
    /// Data cache miss with no MAB entry allocated.
    std::uint64_t is_data_cache_miss_no_mab_allocation : 1;
    /// Linear (virtual) address field is valid.
    std::uint64_t is_data_cache_linear_address_valid : 1;
    /// Physical address field is valid.
    std::uint64_t is_data_cache_physical_address_valid : 1;
    /// L2 DTLB hit with a 1 GB page.
    std::uint64_t is_data_cache_l2_tlb_hit_1g : 1;
    /// Access also missed in the L2 cache.
    std::uint64_t is_l2_miss : 1;
    /// Op is a software prefetch instruction.
    std::uint64_t is_software_prefetch : 1;
    /// Encoded memory access width in bytes (see AMD IBS specification for mapping).
    std::uint64_t op_mem_width : 4;
    /// Number of outstanding memory requests at the time of the data cache miss.
    std::uint64_t num_op_data_cache_miss_open_mem_requests : 6;
    /// Cycles from load dispatch to data return on a data cache miss.
    std::uint64_t data_cache_miss_latency : 16;
    /// Cycles to complete the data TLB refill on a TLB miss.
    std::uint64_t tlb_refill_latency : 16;
  };

  /// Layout of the IBS op MSRs as delivered in the perf raw data buffer.
  struct ExecutionData
  {
    /// IBS_OP_CTL MSR: op sampling control and status flags.
    std::uint64_t _execution_control_register;
    /// IBS_OP_RIP MSR: linear address of the sampled instruction.
    std::uintptr_t _linear_instruction_address;
    /// IBS_OP_DATA MSR: branch classification and latency counters.
    OpData1 _op_data1;
    /// IBS_OP_DATA2 MSR: data source and cache hit/miss status.
    OpData2 _op_data2;
    /// IBS_OP_DATA3 MSR: data cache access details, TLB status, and latency.
    OpData3 _op_data3;
    /// IBS_DC_LINADDR MSR: linear address of the data cache access.
    std::uintptr_t _linear_memory_address;
    /// IBS_DC_PHYSADDR MSR: physical address of the data cache access.
    std::uintptr_t _physical_memory_address;
    /// MSR_AMD64_IBSBRTARGET: branch target address. Only present when the CPU has the
    /// BrnTrgt IBS capability; the kernel may append MSR IBS_OP_DATA4 after it (not mapped here).
    std::uintptr_t _branch_target_address;
  };

  /// The decoders rely on LSB-first bitfield allocation (GCC/Clang on x86-64, the only
  /// platforms with AMD IBS). The asserts lock the MSR image sizes against accidental padding.
  static_assert(sizeof(OpData1) == 8U);
  static_assert(sizeof(OpData2) == 8U);
  static_assert(sizeof(OpData3) == 8U);
  static_assert(sizeof(ExecutionData) == 64U);

  /// Value-initialized IBS op MSRs; bytes not present in the raw buffer remain zero.
  ExecutionData _execution_data{};
  /// Number of payload bytes copied into _execution_data (raw_data.size() - 4, or 0 if too small).
  std::size_t _raw_size{ 0 };
};
}
