#pragma once

#include <cstdint>
#include <list>
#include <perfcpp/analyzer/data_type.h>
#include <perfcpp/sample.h>
#include <set>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace perf::analyzer {
class MemoryAccessResult
{
public:
  explicit MemoryAccessResult(std::vector<DataType>&& result) noexcept
    : _data_types(std::move(result))
  {
  }
  ~MemoryAccessResult() = default;

  [[nodiscard]] const std::vector<DataType>& data_types() const noexcept { return _data_types; }
  [[nodiscard]] std::vector<DataType>& data_types() noexcept { return _data_types; }

  [[nodiscard]] std::string to_string() const;
  [[nodiscard]] std::string to_json() const;
  [[nodiscard]] std::string to_csv(const std::string& data_type_name,
                                   char delimiter = ',',
                                   bool print_header = true) const;
  [[nodiscard]] std::string to_csv(std::string&& data_type_name,
                                   const char delimiter = ',',
                                   const bool print_header = true) const
  {
    return to_csv(data_type_name, delimiter, print_header);
  }

private:
  std::vector<DataType> _data_types;

  class MemberStatistic
  {
  public:
    MemberStatistic() noexcept = default;
    ~MemberStatistic() noexcept = default;

    [[nodiscard]] std::uint64_t loads() const noexcept { return _count_loads; }
    [[nodiscard]] std::uint64_t load_latency() const noexcept
    {
      return _count_loads > 0ULL ? _sum_load_latency / _count_loads : 0ULL;
    }
    [[nodiscard]] std::uint64_t stores() const noexcept { return _count_stores; }
    [[nodiscard]] std::uint64_t store_latency() const noexcept
    {
      return _count_stores > 0ULL ? _sum_store_latency / _count_stores : 0ULL;
    }
    [[nodiscard]] std::uint64_t l1_hits() const noexcept { return _count_l1_hits; }
    [[nodiscard]] std::uint64_t lfb_hits() const noexcept { return _count_lfb_hits; }
    [[nodiscard]] std::uint64_t l2_hits() const noexcept { return _count_l2_hits; }
    [[nodiscard]] std::uint64_t l3_hits() const noexcept { return _count_l3_hits; }
    [[nodiscard]] std::uint64_t l4_hits() const noexcept { return _count_l4_hits; }
    [[nodiscard]] std::uint64_t local_ram_hits() const noexcept { return _count_local_ram_hits; }
    [[nodiscard]] std::uint64_t remote_ram_hits() const noexcept { return _count_remote_ram_hits; }
    [[nodiscard]] std::uint64_t tlb_hits() const noexcept { return _tlb_hits; }
    [[nodiscard]] std::uint64_t tlb_misses() const noexcept { return _tlb_misses; }

    MemberStatistic& operator+=(const Sample& sample) noexcept
    {
      if (!sample.data_src().has_value() || !sample.weight().has_value()) {
        return *this;
      }

      const auto data_src = sample.data_src().value();
      const auto weight = sample.weight().value();

      _count_loads += static_cast<std::uint64_t>(data_src.is_load());
      _sum_load_latency += (static_cast<std::uint64_t>(data_src.is_load()) * weight.cache_latency());
      _count_stores += static_cast<std::uint64_t>(data_src.is_store());
      _sum_store_latency += (static_cast<std::uint64_t>(data_src.is_store()) * weight.cache_latency());
      _count_l1_hits += static_cast<std::uint64_t>(data_src.is_mem_l1());
      _count_lfb_hits += static_cast<std::uint64_t>(data_src.is_mem_lfb());
      _count_l2_hits += static_cast<std::uint64_t>(data_src.is_mem_l2());
      _count_l3_hits += static_cast<std::uint64_t>(data_src.is_mem_l3());
      _count_l4_hits += static_cast<std::uint64_t>(data_src.is_mem_l4());
      _count_local_ram_hits += static_cast<std::uint64_t>(data_src.is_mem_local_ram());
      _count_remote_ram_hits += static_cast<std::uint64_t>(data_src.is_mem_remote_ram());
      _tlb_hits = static_cast<std::uint64_t>(data_src.is_tlb_hit());
      _tlb_misses = static_cast<std::uint64_t>(data_src.is_tlb_miss());
      return *this;
    }

  private:
    std::uint64_t _count_loads{ 0ULL };
    std::uint64_t _sum_load_latency{ 0ULL };
    std::uint64_t _count_stores{ 0ULL };
    std::uint64_t _sum_store_latency{ 0ULL };
    std::uint64_t _count_l1_hits{ 0ULL };
    std::uint64_t _count_lfb_hits{ 0ULL };
    std::uint64_t _count_l2_hits{ 0ULL };
    std::uint64_t _count_l3_hits{ 0ULL };
    std::uint64_t _count_l4_hits{ 0ULL };
    std::uint64_t _count_local_ram_hits{ 0ULL };
    std::uint64_t _count_remote_ram_hits{ 0ULL };
    std::uint64_t _tlb_hits{ 0ULL };
    std::uint64_t _tlb_misses{ 0ULL };
  };
};

class MemoryAccess
{
public:
  MemoryAccess() { _data_type_instances.reserve(128U); }

  ~MemoryAccess() = default;

  /**
   * Adds a data type to the analyzer.
   *
   * @param data_type Data type to add.
   */
  void add(DataType&& data_type);

  /**
   * Annotates the given object with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_object Data object to annotate.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name, T* data_object, std::string&& instance_name = "")
  {
    annotate(data_type_name, std::uintptr_t(data_object), instance_name);
  }

  /**
   * Annotates the given object with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_object Data object to annotate.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name, T* data_object, const std::string& instance_name)
  {
    annotate(data_type_name, std::uintptr_t(data_object), instance_name);
  }

  /**
   * Annotates the given object with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_object Data object to annotate.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name, const T* data_object, std::string&& instance_name = "")
  {
    annotate(data_type_name, std::uintptr_t(data_object), instance_name);
  }

  /**
   * Annotates the given object with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_object Data object to annotate.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name, const T* data_object, const std::string& instance_name)
  {
    annotate(data_type_name, std::uintptr_t(data_object), instance_name);
  }

  /**
   * Annotates the given object with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_object Data object to annotate.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name, const T& data_object, std::string&& instance_name = "")
  {
    annotate(data_type_name, std::uintptr_t(&data_object), instance_name);
  }

  /**
   * Annotates the given object with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_object Data object to annotate.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name, const T& data_object, const std::string& instance_name)
  {
    annotate(data_type_name, std::uintptr_t(&data_object), instance_name);
  }

  /**
   * Annotates the given objects with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_object Array of data objects to annotate.
   * @param size Size of the array.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name,
                const T* data_objects,
                std::size_t size,
                std::string&& instance_name = "")
  {
    for (auto i = 0ULL; i < size; ++i) {
      annotate(data_type_name, std::uintptr_t(&data_objects[i]), instance_name);
    }
  }

  /**
   * Annotates container of objects with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param begin Begin of the container.
   * @param end End of the container.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename I>
  void annotate(const std::string_view data_type_name, I begin, I end, std::string&& instance_name = "")
  {
    annotate(data_type_name, begin, end, instance_name);
  }

  /**
   * Annotates container of objects with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param begin Begin of the container.
   * @param end End of the container.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename I>
  void annotate(const std::string_view data_type_name, I begin, I end, const std::string& instance_name)
  {
    for (auto iterator = begin; iterator != end; ++iterator) {
      annotate(data_type_name, std::uintptr_t(&*iterator), instance_name);
    }
  }

  /**
   * Annotates the given objects with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_objects Data objects to annotate.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name,
                const std::vector<T>& data_objects,
                std::string&& instance_name = "")
  {
    annotate(data_type_name, data_objects.cbegin(), data_objects.cend(), instance_name);
  }

  /**
   * Annotates the given objects with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_objects Data objects to annotate.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name,
                const std::unordered_set<T>& data_objects,
                std::string&& instance_name = "")
  {
    for (const auto& data_object : data_objects) {
      annotate(data_type_name, std::uintptr_t(&data_object), instance_name);
    }
  }

  /**
   * Annotates the given objects with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_objects Data objects to annotate.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name,
                const std::set<T>& data_objects,
                std::string&& instance_name = "")
  {
    for (const auto& data_object : data_objects) {
      annotate(data_type_name, std::uintptr_t(&data_object), instance_name);
    }
  }

  /**
   * Annotates the given objects with the given type.
   *
   * @param data_type_name Name of the (registered) data type.
   * @param data_objects Data objects to annotate.
   * @param instance_name Tag to differentiate multiple instances of the same type (optional).
   */
  template<typename T>
  void annotate(const std::string_view data_type_name,
                const std::list<T>& data_objects,
                std::string&& instance_name = "")
  {
    annotate(data_type_name, data_objects.cbegin(), data_objects.cend(), instance_name);
  }

  /**
   * Maps the given samples (with memory addresses) to data object earlier added to the analyzer.
   *
   * @param samples Samples to map.
   * @return A list of all data types enriched with samples that map to members of the data type.
   */
  MemoryAccessResult map(const std::vector<Sample>& samples);

private:
  /// List of all data types and their instances.
  std::vector<std::pair<DataType, std::unordered_map<std::string, std::vector<std::uintptr_t>>>> _data_type_instances;

  /**
   * Finds a registered data type with the given name.
   *
   * @param data_type_name Name of the data type to lookup.
   * @return Iterator of the data type.
   */
  std::vector<std::pair<DataType, std::unordered_map<std::string, std::vector<std::uintptr_t>>>>::iterator find(
    std::string_view data_type_name) noexcept;

  void annotate(std::string_view data_type_name, std::uintptr_t data_object, const std::string& instance_name);

  /**
   * Fills up the data objects with members in wholes (e.g., space between to members or space between the last member
   * and the end of the data object). This might highlight data objects that are not specified entirely.
   *
   * @param dataType Data type to fill up.
   */
  static void add_empty_attributes(DataType& data_type);

  class DataTypeInstanceComp
  {
  public:
    /**
     * Operator used for performing lower_bound in (instance, data_type) pairs.
     */
    bool operator()(const std::pair<std::uintptr_t, std::reference_wrapper<DataType>>& item,
                    const std::uintptr_t address)
    {
      return std::get<0>(item) <= address;
    }

    /**
     * Operator used for sorting the (instance, data_type) pairs.
     */
    bool operator()(const std::pair<std::uintptr_t, std::reference_wrapper<DataType>>& left,
                    const std::pair<std::uintptr_t, std::reference_wrapper<DataType>>& right)
    {
      return std::get<0>(left) < std::get<0>(right);
    }
  };
};
}