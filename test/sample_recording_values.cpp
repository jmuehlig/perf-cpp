#include <catch2/catch_test_macros.hpp>
#include <perfcpp/sample/branch.hpp>
#include <perfcpp/sample/recording_values.hpp>
#include <perfcpp/sample/registers.hpp>

using Field = perf::SampleRecordingValues::Field;

TEST_CASE("default state", "[SampleRecordingValues]")
{
  const auto values = perf::SampleRecordingValues{};

  SECTION("no field is set")
  {
    /// Spot-check a representative spread across the enum range.
    REQUIRE_FALSE(values.is_set(Field::ThreadId));
    REQUIRE_FALSE(values.is_set(Field::Timestamp));
    REQUIRE_FALSE(values.is_set(Field::LogicalInstructionPointer));
    REQUIRE_FALSE(values.is_set(Field::BranchStack));
    REQUIRE_FALSE(values.is_set(Field::UserRegisters));
    REQUIRE_FALSE(values.is_set(Field::KernelRegisters));
    REQUIRE_FALSE(values.is_set(Field::UserStack));
    REQUIRE_FALSE(values.is_set(Field::PerformanceCounter));
  }

  SECTION("IBS raw-values flag is false")
  {
    REQUIRE_FALSE(values.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("branch mask is zero")
  {
    REQUIRE(values.branch_mask() == 0U);
  }

  SECTION("scalar capacity fields are zero")
  {
    REQUIRE(values.max_user_stack() == 0U);
    REQUIRE(values.max_call_stack() == 0U);
  }

  SECTION("counter name list is empty")
  {
    REQUIRE(values.counters().empty());
  }
}

/// Each setter must activate exactly its own field. Testing against at least one
/// adjacent or similarly-named field catches copy-paste mistakes in the setter body.
TEST_CASE("field isolation – each setter activates only its own field", "[SampleRecordingValues]")
{
  SECTION("thread_id does not bleed into cpu_id")
  {
    auto v = perf::SampleRecordingValues{};
    v.thread_id(true);
    REQUIRE(v.is_set(Field::ThreadId));
    REQUIRE_FALSE(v.is_set(Field::CpuId));
  }

  SECTION("cpu_id does not bleed into thread_id or timestamp")
  {
    auto v = perf::SampleRecordingValues{};
    v.cpu_id(true);
    REQUIRE(v.is_set(Field::CpuId));
    REQUIRE_FALSE(v.is_set(Field::ThreadId));
    REQUIRE_FALSE(v.is_set(Field::Timestamp));
  }

  SECTION("timestamp does not bleed into period")
  {
    auto v = perf::SampleRecordingValues{};
    v.timestamp(true);
    REQUIRE(v.is_set(Field::Timestamp));
    REQUIRE_FALSE(v.is_set(Field::Period));
  }

  SECTION("period does not bleed into timestamp or sample_id")
  {
    auto v = perf::SampleRecordingValues{};
    v.period(true);
    REQUIRE(v.is_set(Field::Period));
    REQUIRE_FALSE(v.is_set(Field::Timestamp));
    REQUIRE_FALSE(v.is_set(Field::SampleId));
  }

  SECTION("sample_id does not bleed into stream_id")
  {
    auto v = perf::SampleRecordingValues{};
    v.sample_id(true);
    REQUIRE(v.is_set(Field::SampleId));
    REQUIRE_FALSE(v.is_set(Field::StreamId));
  }

  SECTION("stream_id does not bleed into sample_id")
  {
    auto v = perf::SampleRecordingValues{};
    v.stream_id(true);
    REQUIRE(v.is_set(Field::StreamId));
    REQUIRE_FALSE(v.is_set(Field::SampleId));
  }

  SECTION("logical_instruction_pointer does not bleed into physical_instruction_pointer")
  {
    auto v = perf::SampleRecordingValues{};
    v.logical_instruction_pointer(true);
    REQUIRE(v.is_set(Field::LogicalInstructionPointer));
    REQUIRE_FALSE(v.is_set(Field::PhysicalInstructionPointer));
  }

  SECTION("physical_instruction_pointer does not bleed into logical_instruction_pointer")
  {
    auto v = perf::SampleRecordingValues{};
    v.physical_instruction_pointer(true);
    REQUIRE(v.is_set(Field::PhysicalInstructionPointer));
    REQUIRE_FALSE(v.is_set(Field::LogicalInstructionPointer));
  }

  SECTION("data_access_latency does not bleed into data_tlb_latency")
  {
    auto v = perf::SampleRecordingValues{};
    v.data_access_latency(true);
    REQUIRE(v.is_set(Field::DataAccessLatency));
    REQUIRE_FALSE(v.is_set(Field::DataTLBLatency));
  }

  SECTION("data_tlb_latency does not bleed into data_access_latency")
  {
    auto v = perf::SampleRecordingValues{};
    v.data_tlb_latency(true);
    REQUIRE(v.is_set(Field::DataTLBLatency));
    REQUIRE_FALSE(v.is_set(Field::DataAccessLatency));
  }

  SECTION("user_registers does not bleed into kernel_registers")
  {
    auto v = perf::SampleRecordingValues{};
    v.user_registers(std::vector<perf::Registers::x86>{ perf::Registers::x86::IP });
    REQUIRE(v.is_set(Field::UserRegisters));
    REQUIRE_FALSE(v.is_set(Field::KernelRegisters));
  }

  SECTION("kernel_registers does not bleed into user_registers")
  {
    auto v = perf::SampleRecordingValues{};
    v.kernel_registers(std::vector<perf::Registers::x86>{ perf::Registers::x86::IP });
    REQUIRE(v.is_set(Field::KernelRegisters));
    REQUIRE_FALSE(v.is_set(Field::UserRegisters));
  }
}

TEST_CASE("toggling a field off disables it", "[SampleRecordingValues]")
{
  auto v = perf::SampleRecordingValues{};
  v.timestamp(true);
  REQUIRE(v.is_set(Field::Timestamp));

  v.timestamp(false);
  REQUIRE_FALSE(v.is_set(Field::Timestamp));
}

/// Each IBS-related field must individually trigger the flag. Listing them one by
/// one (rather than testing the combined OR) catches the case where a single field
/// was accidentally omitted from the implementation.
TEST_CASE("is_need_raw_values_for_ibs_decoding – each IBS field triggers the flag", "[SampleRecordingValues]")
{
  SECTION("physical_instruction_pointer")
  {
    auto v = perf::SampleRecordingValues{};
    v.physical_instruction_pointer(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("instruction_type")
  {
    auto v = perf::SampleRecordingValues{};
    v.instruction_type(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("branch_type (IBS op field, distinct from branch_stack)")
  {
    auto v = perf::SampleRecordingValues{};
    v.branch_type(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("instruction_latency")
  {
    auto v = perf::SampleRecordingValues{};
    v.instruction_latency(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("instruction_cache")
  {
    auto v = perf::SampleRecordingValues{};
    v.instruction_cache(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("instruction_tlb")
  {
    auto v = perf::SampleRecordingValues{};
    v.instruction_tlb(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("instruction_fetch")
  {
    auto v = perf::SampleRecordingValues{};
    v.instruction_fetch(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("data_access_width")
  {
    auto v = perf::SampleRecordingValues{};
    v.data_access_width(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("data_access_misaligned")
  {
    auto v = perf::SampleRecordingValues{};
    v.data_access_misaligned(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("mhb_allocations")
  {
    auto v = perf::SampleRecordingValues{};
    v.mhb_allocations(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("data_tlb_latency")
  {
    auto v = perf::SampleRecordingValues{};
    v.data_tlb_latency(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("data_tlb_page_size")
  {
    auto v = perf::SampleRecordingValues{};
    v.data_tlb_page_size(true);
    REQUIRE(v.is_need_raw_values_for_ibs_decoding());
  }
}

/// These fields are commonly confused with IBS fields by name. None of them should
/// require raw values. Verifying them separately avoids the case where the flag
/// is spuriously set for non-IBS fields.
TEST_CASE("is_need_raw_values_for_ibs_decoding – non-IBS fields do not trigger the flag", "[SampleRecordingValues]")
{
  SECTION("timestamp")
  {
    auto v = perf::SampleRecordingValues{};
    v.timestamp(true);
    REQUIRE_FALSE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("cpu_id")
  {
    auto v = perf::SampleRecordingValues{};
    v.cpu_id(true);
    REQUIRE_FALSE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("logical_instruction_pointer (not the same as physical_instruction_pointer)")
  {
    auto v = perf::SampleRecordingValues{};
    v.logical_instruction_pointer(true);
    REQUIRE_FALSE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("logical_memory_address")
  {
    auto v = perf::SampleRecordingValues{};
    v.logical_memory_address(true);
    REQUIRE_FALSE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("data_access_latency (not the same as data_tlb_latency)")
  {
    auto v = perf::SampleRecordingValues{};
    v.data_access_latency(true);
    REQUIRE_FALSE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("branch_stack (LBR sampling, not IBS branch_type)")
  {
    auto v = perf::SampleRecordingValues{};
    v.branch_stack({ perf::BranchType::User, perf::BranchType::Conditional });
    REQUIRE_FALSE(v.is_need_raw_values_for_ibs_decoding());
  }

  SECTION("user_registers")
  {
    auto v = perf::SampleRecordingValues{};
    v.user_registers(std::vector<perf::Registers::x86>{ perf::Registers::x86::IP });
    REQUIRE_FALSE(v.is_need_raw_values_for_ibs_decoding());
  }
}

TEST_CASE("user_stack edge cases", "[SampleRecordingValues]")
{
  SECTION("size zero does not enable the field")
  {
    auto v = perf::SampleRecordingValues{};
    v.user_stack(0U);
    REQUIRE_FALSE(v.is_set(Field::UserStack));
    REQUIRE(v.max_user_stack() == 0U);
  }

  SECTION("non-zero size enables the field and stores the value")
  {
    auto v = perf::SampleRecordingValues{};
    v.user_stack(512U);
    REQUIRE(v.is_set(Field::UserStack));
    REQUIRE(v.max_user_stack() == 512U);
  }
}

TEST_CASE("callchain overloads", "[SampleRecordingValues]")
{
  SECTION("bool overload enables field, leaves max_call_stack at zero")
  {
    auto v = perf::SampleRecordingValues{};
    v.callchain(true);
    REQUIRE(v.is_set(Field::Callchain));
    REQUIRE(v.max_call_stack() == 0U);
  }

  SECTION("depth overload enables field and stores depth")
  {
    auto v = perf::SampleRecordingValues{};
    v.callchain(std::uint16_t{ 32U });
    REQUIRE(v.is_set(Field::Callchain));
    REQUIRE(v.max_call_stack() == 32U);
  }
}

TEST_CASE("registers enable/disable", "[SampleRecordingValues]")
{
  SECTION("empty user register list does not enable the field")
  {
    auto v = perf::SampleRecordingValues{};
    v.user_registers(std::vector<perf::Registers::x86>{});
    REQUIRE_FALSE(v.is_set(Field::UserRegisters));
  }

  SECTION("non-empty user register list enables the field")
  {
    auto v = perf::SampleRecordingValues{};
    v.user_registers(std::vector<perf::Registers::x86>{ perf::Registers::x86::AX });
    REQUIRE(v.is_set(Field::UserRegisters));
  }

  SECTION("empty kernel register list does not enable the field")
  {
    auto v = perf::SampleRecordingValues{};
    v.kernel_registers(std::vector<perf::Registers::x86>{});
    REQUIRE_FALSE(v.is_set(Field::KernelRegisters));
  }

  SECTION("non-empty kernel register list enables the field")
  {
    auto v = perf::SampleRecordingValues{};
    v.kernel_registers(std::vector<perf::Registers::x86>{ perf::Registers::x86::IP });
    REQUIRE(v.is_set(Field::KernelRegisters));
  }
}

TEST_CASE("performance counter list enable/disable", "[SampleRecordingValues]")
{
  SECTION("empty counter list does not enable the field")
  {
    auto v = perf::SampleRecordingValues{};
    v.counter({});
    REQUIRE_FALSE(v.is_set(Field::PerformanceCounter));
    REQUIRE(v.counters().empty());
  }

  SECTION("non-empty counter list enables the field and stores the names")
  {
    auto v = perf::SampleRecordingValues{};
    v.counter({ "instructions", "cycles" });
    REQUIRE(v.is_set(Field::PerformanceCounter));
    REQUIRE(v.counters().size() == 2U);
    REQUIRE(v.counters()[0] == "instructions");
    REQUIRE(v.counters()[1] == "cycles");
  }
}

TEST_CASE("branch_stack mask composition", "[SampleRecordingValues]")
{
  SECTION("empty branch type list disables the field and yields mask zero")
  {
    auto v = perf::SampleRecordingValues{};
    v.branch_stack({});
    REQUIRE_FALSE(v.is_set(Field::BranchStack));
    REQUIRE(v.branch_mask() == 0U);
  }

  SECTION("single branch type yields a non-zero mask")
  {
    auto v = perf::SampleRecordingValues{};
    v.branch_stack({ perf::BranchType::User });
    REQUIRE(v.is_set(Field::BranchStack));
    REQUIRE(v.branch_mask() != 0U);
  }

  SECTION("two distinct branch types produce different individual masks")
  {
    auto user = perf::SampleRecordingValues{};
    user.branch_stack({ perf::BranchType::User });

    auto cond = perf::SampleRecordingValues{};
    cond.branch_stack({ perf::BranchType::Conditional });

    /// Distinct types must map to distinct bits.
    REQUIRE(user.branch_mask() != cond.branch_mask());
  }

  SECTION("combined mask contains the bits of each individual type")
  {
    auto user = perf::SampleRecordingValues{};
    user.branch_stack({ perf::BranchType::User });

    auto cond = perf::SampleRecordingValues{};
    cond.branch_stack({ perf::BranchType::Conditional });

    auto both = perf::SampleRecordingValues{};
    both.branch_stack({ perf::BranchType::User, perf::BranchType::Conditional });

    REQUIRE((both.branch_mask() & user.branch_mask()) == user.branch_mask());
    REQUIRE((both.branch_mask() & cond.branch_mask()) == cond.branch_mask());
  }
}

TEST_CASE("fluent API – chained setters all take effect", "[SampleRecordingValues]")
{
  const auto v = perf::SampleRecordingValues{}.timestamp(true).cpu_id(true).logical_instruction_pointer(true);

  REQUIRE(v.is_set(Field::Timestamp));
  REQUIRE(v.is_set(Field::CpuId));
  REQUIRE(v.is_set(Field::LogicalInstructionPointer));

  /// Setting one field via chaining must not silently disable another.
  REQUIRE_FALSE(v.is_set(Field::ThreadId));
  REQUIRE_FALSE(v.is_set(Field::Period));
}
