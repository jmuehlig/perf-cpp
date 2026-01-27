#include <algorithm>
#include <perfcpp/sample_result.h>

void
perf::SampleResult::filter(std::function<bool(const Sample&)> filter)
{
  this->_samples.erase(std::remove_if(this->_samples.begin(),
                                      this->_samples.end(),
                                      [&filter](const auto& sample) { return !filter(sample); }),
                       this->_samples.end());
}

void
perf::SampleResult::to_csv(std::string&& file_name) const
{
  auto file_stream = std::ofstream(file_name);
  auto csv_writer = CSVWriter{ file_stream, this->_sample_recording_values };

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
  csv_writer.write_header(SampleRecordingValues::Field::LogicalInstructionPointer, "logical_instruction_pointer");
  csv_writer.write_header(SampleRecordingValues::Field::PhysicalInstructionPointer, "physical_instruction_pointer");
  csv_writer.write_header(SampleRecordingValues::Field::LogicalInstructionPointer, "is_instruction_pointer_exact");
  csv_writer.write_header(SampleRecordingValues::Field::DataSource, "is_instruction_locked");

  /// Samples
  for (const auto& sample : this->_samples) {
    file_stream << '\n';

    /// Metadata
    file_stream << sample.metadata().mode_as_string().value_or("");
    csv_writer.write_value(SampleRecordingValues::Field::Id, sample.metadata().sample_id());
    csv_writer.write_value(SampleRecordingValues::Field::StreamId, sample.metadata().stream_id());
    csv_writer.write_value(SampleRecordingValues::Field::Timestamp, sample.metadata().timestamp());
    csv_writer.write_value(SampleRecordingValues::Field::Period, sample.metadata().period());
    csv_writer.write_value(SampleRecordingValues::Field::CpuId, sample.metadata().cpu_id());
    csv_writer.write_value(SampleRecordingValues::Field::ThreadId, sample.metadata().process_id());
    csv_writer.write_value(SampleRecordingValues::Field::ThreadId, sample.metadata().thread_id());

    /// Instruction execution
    csv_writer.write_value(SampleRecordingValues::Field::LogicalInstructionPointer,
                           sample.instruction_execution().logical_instruction_pointer(),
                           true);
    csv_writer.write_value(SampleRecordingValues::Field::PhysicalInstructionPointer,
                           sample.instruction_execution().physical_instruction_pointer(),
                           true);
    csv_writer.write_value(SampleRecordingValues::Field::LogicalInstructionPointer,
                           sample.instruction_execution().is_instruction_pointer_exact());
    csv_writer.write_value(SampleRecordingValues::Field::DataSource, sample.instruction_execution().is_locked());
  }

  file_stream << std::flush;
}