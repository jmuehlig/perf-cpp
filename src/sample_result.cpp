#include <algorithm>
#include <perfcpp/sample_result.h>

#include "perfcpp/hardware_info.h"

void
perf::SampleResult::filter(std::function<bool(const Sample&)> filter)
{
  this->_samples.erase(std::remove_if(this->_samples.begin(),
                                      this->_samples.end(),
                                      [&filter](const auto& sample) { return !filter(sample); }),
                       this->_samples.end());
}

void
perf::SampleResult::to_csv(std::string&& file_name, const char delimiter, const char list_delimiter) const
{
  auto file_stream = std::ofstream(file_name);
  auto csv_writer = CSVWriter{ file_stream, this->_sample_recording_values, delimiter, list_delimiter };

  /// Header: Metadata
  file_stream << "mode";
  csv_writer.write_header(SampleRecordingValues::Field::Id, "id");
  csv_writer.write_header(SampleRecordingValues::Field::StreamId, "stream_id");
  csv_writer.write_header(SampleRecordingValues::Field::Timestamp, "timestamp");
  csv_writer.write_header(SampleRecordingValues::Field::Period, "period");
  csv_writer.write_header(SampleRecordingValues::Field::CpuId, "cpu_id");
  csv_writer.write_header(SampleRecordingValues::Field::ThreadId, "process_id");
  csv_writer.write_header(SampleRecordingValues::Field::ThreadId, "thread_id");

  /// Header: Instruction Execution
  csv_writer.write_header(SampleRecordingValues::Field::InstructionType, "instruction_type");
  csv_writer.write_header(SampleRecordingValues::Field::LogicalInstructionPointer, "logical_instruction_pointer");
  csv_writer.write_header(SampleRecordingValues::Field::LogicalInstructionPointer, "is_instruction_pointer_exact");
  csv_writer.write_header(SampleRecordingValues::Field::PhysicalInstructionPointer, "physical_instruction_pointer");
  csv_writer.write_header(SampleRecordingValues::Field::InstructionCache, "instruction_l1i_miss");
  csv_writer.write_header(SampleRecordingValues::Field::InstructionCache, "instruction_l2_miss");
  csv_writer.write_header(SampleRecordingValues::Field::InstructionCache, "instruction_l2_miss");
  csv_writer.write_header(SampleRecordingValues::Field::InstructionTLB, "instruction_itlb_miss");
  csv_writer.write_header(SampleRecordingValues::Field::InstructionTLB, "instruction_itlb_size");
  csv_writer.write_header(SampleRecordingValues::Field::InstructionTLB, "instruction_stlb_miss");

  if (HardwareInfo::is_intel()) {
    csv_writer.write_header(SampleRecordingValues::Field::InstructionLatency, "instruction_retirement_latency");
  } else if (HardwareInfo::is_amd()) {
    csv_writer.write_header(SampleRecordingValues::Field::InstructionLatency, "uop_tag_to_retirement_latency");
    csv_writer.write_header(SampleRecordingValues::Field::InstructionLatency, "uop_tag_to_completion");
    csv_writer.write_header(SampleRecordingValues::Field::InstructionLatency, "uop_completion_to_retirement");
    csv_writer.write_header(SampleRecordingValues::Field::InstructionLatency, "instruction_fetch_latency");
  }

  csv_writer.write_header(SampleRecordingValues::Field::BranchType, "branch_type");
  csv_writer.write_header(SampleRecordingValues::Field::HardwareTransactionAbort, "tx_is_elision");
  csv_writer.write_header(SampleRecordingValues::Field::HardwareTransactionAbort, "tx_is_generic");
  csv_writer.write_header(SampleRecordingValues::Field::HardwareTransactionAbort, "tx_is_synchronous_abort");
  csv_writer.write_header(SampleRecordingValues::Field::HardwareTransactionAbort, "tx_is_retryable");
  csv_writer.write_header(SampleRecordingValues::Field::HardwareTransactionAbort, "tx_is_memory_conflict");
  csv_writer.write_header(SampleRecordingValues::Field::HardwareTransactionAbort, "tx_is_write_capacity_conflict");
  csv_writer.write_header(SampleRecordingValues::Field::HardwareTransactionAbort, "tx_is_read_capacity_conflict");
  csv_writer.write_header(SampleRecordingValues::Field::HardwareTransactionAbort, "tx_user_code");
  csv_writer.write_header(SampleRecordingValues::Field::CodePageSize, "code_page_size");
  csv_writer.write_header(SampleRecordingValues::Field::Callchain, "callchain");

  /// Samples
  for (const auto& sample : this->_samples) {
    file_stream << '\n';

    /// Metadata
    file_stream << to_string(sample.metadata().mode());
    csv_writer.write_value(SampleRecordingValues::Field::Id, sample.metadata().sample_id());
    csv_writer.write_value(SampleRecordingValues::Field::StreamId, sample.metadata().stream_id());
    csv_writer.write_value(SampleRecordingValues::Field::Timestamp, sample.metadata().timestamp());
    csv_writer.write_value(SampleRecordingValues::Field::Period, sample.metadata().period());
    csv_writer.write_value(SampleRecordingValues::Field::CpuId, sample.metadata().cpu_id());
    csv_writer.write_value(SampleRecordingValues::Field::ThreadId, sample.metadata().process_id());
    csv_writer.write_value(SampleRecordingValues::Field::ThreadId, sample.metadata().thread_id());

    /// Instruction execution
    csv_writer.write_value(SampleRecordingValues::Field::InstructionType, sample.instruction_execution().type());
    csv_writer.write_value(SampleRecordingValues::Field::LogicalInstructionPointer,
                           sample.instruction_execution().logical_instruction_pointer(),
                           true);
    csv_writer.write_value(SampleRecordingValues::Field::LogicalInstructionPointer,
                           sample.instruction_execution().is_instruction_pointer_exact());
    csv_writer.write_value(SampleRecordingValues::Field::PhysicalInstructionPointer,
                           sample.instruction_execution().physical_instruction_pointer(),
                           true);

    const auto& instruction_cache = sample.instruction_execution().cache();
    csv_writer.write_value(
      SampleRecordingValues::Field::InstructionCache, instruction_cache, [](const auto& c) { return c.is_l1_miss(); });
    csv_writer.write_value(
      SampleRecordingValues::Field::InstructionCache, instruction_cache, [](const auto& c) { return c.is_l2_miss(); });
    csv_writer.write_value(
      SampleRecordingValues::Field::InstructionCache, instruction_cache, [](const auto& c) { return c.is_l3_miss(); });

    const auto& instruction_tlb = sample.instruction_execution().tlb();
    csv_writer.write_value(
      SampleRecordingValues::Field::InstructionTLB, instruction_tlb, [](const auto& t) { return t.is_l1_miss(); });
    csv_writer.write_value(
      SampleRecordingValues::Field::InstructionTLB, instruction_tlb, [](const auto& t) { return t.l1_page_size(); });
    csv_writer.write_value(
      SampleRecordingValues::Field::InstructionTLB, instruction_tlb, [](const auto& t) { return t.is_l2_miss(); });

    const auto& instruction_latency = sample.instruction_execution().latency();
    if (HardwareInfo::is_intel()) {
      csv_writer.write_value(SampleRecordingValues::Field::InstructionLatency,
                             instruction_latency.instruction_retirement());
    } else if (HardwareInfo::is_amd()) {
      csv_writer.write_value(SampleRecordingValues::Field::InstructionLatency,
                             instruction_latency.uop_tag_to_retirement());
      csv_writer.write_value(SampleRecordingValues::Field::InstructionLatency,
                             instruction_latency.uop_tag_to_completion());
      csv_writer.write_value(SampleRecordingValues::Field::InstructionLatency,
                             instruction_latency.uop_completion_to_retirement());
      csv_writer.write_value(SampleRecordingValues::Field::InstructionLatency, instruction_latency.fetch());
    }

    csv_writer.write_value(SampleRecordingValues::Field::BranchType, sample.instruction_execution().branch_type());

    const auto& hardware_transaction_abort = sample.instruction_execution().hardware_transaction_abort();
    csv_writer.write_value(SampleRecordingValues::Field::HardwareTransactionAbort,
                           hardware_transaction_abort,
                           [](const auto& t) { return t.is_elision_transaction(); });
    csv_writer.write_value(SampleRecordingValues::Field::HardwareTransactionAbort,
                           hardware_transaction_abort,
                           [](const auto& t) { return t.is_generic_transaction(); });
    csv_writer.write_value(SampleRecordingValues::Field::HardwareTransactionAbort,
                           hardware_transaction_abort,
                           [](const auto& t) { return t.is_synchronous_abort(); });
    csv_writer.write_value(SampleRecordingValues::Field::HardwareTransactionAbort,
                           hardware_transaction_abort,
                           [](const auto& t) { return t.is_retryable(); });
    csv_writer.write_value(SampleRecordingValues::Field::HardwareTransactionAbort,
                           hardware_transaction_abort,
                           [](const auto& t) { return t.is_due_to_memory_conflict(); });
    csv_writer.write_value(SampleRecordingValues::Field::HardwareTransactionAbort,
                           hardware_transaction_abort,
                           [](const auto& t) { return t.is_due_to_write_capacity_conflict(); });
    csv_writer.write_value(SampleRecordingValues::Field::HardwareTransactionAbort,
                           hardware_transaction_abort,
                           [](const auto& t) { return t.is_due_to_read_capacity_conflict(); });
    csv_writer.write_value(SampleRecordingValues::Field::HardwareTransactionAbort,
                           hardware_transaction_abort,
                           [](const auto& t) { return t.user_specified_code(); });

    csv_writer.write_value(SampleRecordingValues::Field::Callchain, sample.instruction_execution().callchain(), true);
    csv_writer.write_value(SampleRecordingValues::Field::CodePageSize, sample.instruction_execution().page_size());
  }

  file_stream << std::flush;
}