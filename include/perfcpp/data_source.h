#pragma once

#include <cstdint>
#include <linux/perf_event.h>

namespace perf
{
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
}