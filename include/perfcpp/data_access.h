#pragma once

#include <cstdint>
#include <optional>

namespace perf {
class DataAccess
{
public:
  class Source
  {
  public:
    /**
     * Set whether L1 data cache is hit.
     * @param is_l1d_hit L1 data cache hit indicator.
     */
    void is_l1d_hit(const bool is_l1d_hit) noexcept { _is_l1d_hit = is_l1d_hit; }

    /**
     * Set whether MHB is allocated.
     * @param is_mhb_allocated MHB allocation indicator.
     */
    void is_mhb_allocated(const bool is_mhb_allocated) noexcept { _is_mhb_allocated = is_mhb_allocated; }

    /**
     * Set whether MHB is hit.
     * @param is_mhb_hit MHB hit indicator.
     */
    void is_mhb_hit(const bool is_mhb_hit) noexcept { _is_mhb_hit = is_mhb_hit; }

    /**
     * Set the number of MHB slots allocated.
     * @param num_mhb_slots_allocated Number of MHB slots allocated.
     */
    void num_mhb_slots_allocated(const std::uint8_t num_mhb_slots_allocated) noexcept
    {
      _num_mhb_slots_allocated = num_mhb_slots_allocated;
    }

    /**
     * Set whether L2 cache is hit.
     * @param is_l2_hit L2 cache hit indicator.
     */
    void is_l2_hit(const bool is_l2_hit) noexcept { _is_l2_hit = is_l2_hit; }

    /**
     * Set whether L3 cache is hit.
     * @param is_l3_hit L3 cache hit indicator.
     */
    void is_l3_hit(const bool is_l3_hit) noexcept { _is_l3_hit = is_l3_hit; }

    /**
     * Set whether L4 cache is hit.
     * @param is_l4_hit L4 cache hit indicator.
     */
    void is_l4_hit(const bool is_l4_hit) noexcept { _is_l4_hit = is_l4_hit; }

    /**
     * Set whether memory is hit.
     * @param is_memory_hit Memory hit indicator.
     */
    void is_memory_hit(const bool is_memory_hit) noexcept { _is_memory_hit = is_memory_hit; }

    /**
     * Set whether the source is remote.
     * @param is_remote Remote source indicator.
     */
    void is_remote(const bool is_remote) noexcept { _is_remote = is_remote; }

    /**
     * Set the number of remote hops.
     * @param remote_hops Number of remote hops.
     */
    void remote_hops(const std::uint8_t remote_hops) noexcept { _remote_hops = remote_hops; }

    /**
     * Set whether the memory is uncachable.
     * @param is_uncachable_memory Uncachable memory indicator.
     */
    void is_uncachable_memory(const bool is_uncachable_memory) noexcept
    {
      _is_uncachable_memory = is_uncachable_memory;
    }

    /**
     * Set whether the memory is write-combine.
     * @param is_write_combine_memory Write-combine memory indicator.
     */
    void is_write_combine_memory(const bool is_write_combine_memory) noexcept
    {
      _is_write_combine_memory = is_write_combine_memory;
    }

    /**
     * Set whether there is a misalignment penalty.
     * @param is_misalign_penalty Misalignment penalty indicator.
     */
    void is_misalign_penalty(const bool is_misalign_penalty) noexcept { _is_misalign_penalty = is_misalign_penalty; }

    /**
     * Set the width of the access.
     *
     * @param access_width Width of the access.
     */
    void access_width(std::uint8_t access_width) noexcept { _access_width = access_width; }

    /**
     * @return L1 data cache hit indicator.
     */
    [[nodiscard]] bool is_l1d_hit() const noexcept { return _is_l1d_hit; }

    /**
     * @return MHB allocation indicator, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<bool> is_mhb_allocated() const noexcept { return _is_mhb_allocated; }

    /**
     * @return MHB hit indicator, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<bool> is_mhb_hit() const noexcept { return _is_mhb_hit; }

    /**
     * @return Number of MHB slots allocated, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<std::uint8_t> num_mhb_slots_allocated() const noexcept
    {
      return _num_mhb_slots_allocated;
    }

    /**
     * @return L2 cache hit indicator.
     */
    [[nodiscard]] bool is_l2_hit() const noexcept { return _is_l2_hit; }

    /**
     * @return L3 cache hit indicator.
     */
    [[nodiscard]] bool is_l3_hit() const noexcept { return _is_l3_hit; }

    /**
     * @return L4 cache hit indicator.
     */
    [[nodiscard]] bool is_l4_hit() const noexcept { return _is_l4_hit; }

    /**
     * @return Memory hit indicator.
     */
    [[nodiscard]] bool is_memory_hit() const noexcept { return _is_memory_hit; }

    /**
     * @return Remote source indicator.
     */
    [[nodiscard]] bool is_remote() const noexcept { return _is_remote; }

    /**
     * @return True, if the access was on the same node but a remote core.
     */
    [[nodiscard]] std::optional<bool> is_same_node_remote_core() const noexcept
    {
      if (!_remote_hops.has_value()) {
        return std::nullopt;
      }

      return _remote_hops.value() == 0U;
    }

    /**
     * @return True, if the access was on the same socket but a remote node.
     */
    [[nodiscard]] std::optional<bool> is_same_socket_remote_node() const noexcept
    {
      if (!_remote_hops.has_value()) {
        return std::nullopt;
      }

      return _remote_hops.value() == 1U;
    }

    /**
     * @return True, if the access was on the same board but a remote socket.
     */
    [[nodiscard]] std::optional<bool> is_same_board_remote_socket() const noexcept
    {
      if (!_remote_hops.has_value()) {
        return std::nullopt;
      }
      return _remote_hops.value() == 2U;
    }

    /**
     * @return True, if the access was on a remote board.
     */
    [[nodiscard]] std::optional<bool> is_remote_board() const noexcept
    {
      if (!_remote_hops.has_value()) {
        return std::nullopt;
      }
      return _remote_hops.value() == 3U;
    }

    /**
     * @return Uncachable memory indicator, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<bool> is_uncachable_memory() const noexcept { return _is_uncachable_memory; }

    /**
     * @return Write-combine memory indicator, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<bool> is_write_combine_memory() const noexcept { return _is_write_combine_memory; }

    /**
     * @return Misalignment penalty indicator, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<bool> is_misalign_penalty() const noexcept { return _is_misalign_penalty; }

    /**
     * @return Width of the access in bytes, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<std::uint8_t> access_width() const noexcept { return _access_width; }

  private:
    bool _is_l1d_hit{ false };
    std::optional<bool> _is_mhb_allocated{ std::nullopt };
    std::optional<bool> _is_mhb_hit{ std::nullopt };
    std::optional<std::uint8_t> _num_mhb_slots_allocated{ std::nullopt };
    bool _is_l2_hit{ false };
    bool _is_l3_hit{ false };
    bool _is_l4_hit{ false };
    bool _is_memory_hit{ false };
    bool _is_remote{ false };
    std::optional<std::uint8_t> _remote_hops{ std::nullopt };
    std::optional<bool> _is_uncachable_memory{ std::nullopt };
    std::optional<bool> _is_write_combine_memory{ std::nullopt };
    std::optional<bool> _is_misalign_penalty{ std::nullopt };
    std::optional<std::uint8_t> _access_width{ std::nullopt };
  };

  class TLB
  {
  public:
    /**
     * Set whether L1 TLB is hit.
     * @param is_l1_hit L1 TLB hit indicator.
     */
    void is_l1_hit(const bool is_l1_hit) noexcept { _is_l1_hit = is_l1_hit; }

    /**
     * Set whether L2 TLB is hit.
     * @param is_l2_hit L2 TLB hit indicator.
     */
    void is_l2_hit(const bool is_l2_hit) noexcept { _is_l2_hit = is_l2_hit; }

    /**
     * Set L1 TLB page size.
     * @param l1_page_size L1 TLB page size.
     */
    void l1_page_size(const std::uint64_t l1_page_size) noexcept { _l1_page_size = l1_page_size; }

    /**
     * Set L2 TLB page size.
     * @param l2_page_size L2 TLB page size.
     */
    void l2_page_size(const std::uint64_t l2_page_size) noexcept { _l2_page_size = l2_page_size; }

    /**
     * @return L1 TLB hit indicator, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<bool> is_l1_hit() const noexcept { return _is_l1_hit; }

    /**
     * @return L2 TLB hit indicator, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<bool> is_l2_hit() const noexcept { return _is_l2_hit; }

    /**
     * @return L1 TLB page size, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<std::uint64_t> l1_page_size() const noexcept { return _l1_page_size; }

    /**
     * @return L2 TLB page size, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<std::uint64_t> l2_page_size() const noexcept { return _l2_page_size; }

  private:
    std::optional<bool> _is_l1_hit{ std::nullopt };
    std::optional<bool> _is_l2_hit{ std::nullopt };
    std::optional<std::uint64_t> _l1_page_size{ std::nullopt };
    std::optional<std::uint64_t> _l2_page_size{ std::nullopt };
  };

  class Latency
  {
  public:
    /**
     * Set the data access latency.
     * @param data_access Data access latency.
     */
    void data_access(const std::uint32_t data_access) noexcept { _data_access = data_access; }

    /**
     * Set the cache miss latency.
     * @param cache_miss Cache miss latency.
     */
    void cache_miss(const std::uint32_t cache_miss) noexcept { _cache_miss = cache_miss; }

    /**
     * Set the DTLB refill latency.
     * @param dtlb_refill DTLB refill latency.
     */
    void dtlb_refill(const std::uint32_t dtlb_refill) noexcept { _dtlb_refill = dtlb_refill; }

    /**
     * @return Data access latency, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<std::uint32_t> data_access() const noexcept { return _data_access; }

    /**
     * @return Cache miss latency, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<std::uint32_t> cache_miss() const noexcept { return _cache_miss; }

    /**
     * @return DTLB refill latency, if available. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<std::uint32_t> dtlb_refill() const noexcept { return _dtlb_refill; }

  private:
    std::optional<std::uint32_t> _data_access{ std::nullopt };
    std::optional<std::uint32_t> _cache_miss{ std::nullopt };
    std::optional<std::uint32_t> _dtlb_refill{ std::nullopt };
  };

  void logical_memory_address(const std::uintptr_t logical_memory_address) noexcept
  {
    _logical_memory_address = logical_memory_address;
  }
  void physical_memory_address(const std::uintptr_t physical_memory_address) noexcept
  {
    _physical_memory_address = physical_memory_address;
  }

  void source(const Source source) noexcept { _source.emplace(source); }

  [[nodiscard]] std::optional<std::uintptr_t> logical_memory_address() const noexcept
  {
    return _logical_memory_address;
  }
  [[nodiscard]] std::optional<std::uintptr_t> physical_memory_address() const noexcept
  {
    return _physical_memory_address;
  }

  [[nodiscard]] const std::optional<Source>& source() const noexcept { return _source; }
  [[nodiscard]] std::optional<Source>& source() noexcept { return _source; }

  [[nodiscard]] const TLB& tlb() const noexcept { return _tlb; }
  [[nodiscard]] TLB& tlb() noexcept { return _tlb; }

  [[nodiscard]] const Latency& latency() const noexcept { return _latency; }
  [[nodiscard]] Latency& latency() noexcept { return _latency; }

private:
  std::optional<std::uintptr_t> _logical_memory_address;
  std::optional<std::uintptr_t> _physical_memory_address;
  std::optional<Source> _source;
  TLB _tlb;
  Latency _latency;
};
}