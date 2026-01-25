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
  csv_writer.write_header(PERF_SAMPLE_ID, "id");
  csv_writer.write_header(PERF_SAMPLE_STREAM_ID, "stream_id");
  csv_writer.write_header(PERF_SAMPLE_TIME, "timestamp");
  csv_writer.write_header(PERF_SAMPLE_PERIOD, "period");
  csv_writer.write_header(PERF_SAMPLE_CPU, "cpu_id");
  csv_writer.write_header(PERF_SAMPLE_TID, "process_id");
  csv_writer.write_header(PERF_SAMPLE_TID, "thread_id");
  /// Header: Instruction Execution
  csv_writer.write_header(PERF_SAMPLE_IP, "logical_instruction_pointer");
  csv_writer.write_header(PERF_SAMPLE_IP, "physical_instruction_pointer");
  csv_writer.write_header(PERF_SAMPLE_IP, "is_instruction_pointer_exact");
  csv_writer.write_header(PERF_SAMPLE_DATA_SRC, "is_instruction_locked");

  /// Samples
  for (const auto& sample : this->_samples) {
    file_stream << '\n';

    /// Metadata
    file_stream << sample.metadata().mode_as_string().value_or("");
    csv_writer.write_value(PERF_SAMPLE_ID, sample.metadata().sample_id());
    csv_writer.write_value(PERF_SAMPLE_STREAM_ID, sample.metadata().stream_id());
    csv_writer.write_value(PERF_SAMPLE_TIME, sample.metadata().timestamp());
    csv_writer.write_value(PERF_SAMPLE_PERIOD, sample.metadata().period());
    csv_writer.write_value(PERF_SAMPLE_CPU, sample.metadata().cpu_id());
    csv_writer.write_value(PERF_SAMPLE_TID, sample.metadata().process_id());
    csv_writer.write_value(PERF_SAMPLE_TID, sample.metadata().thread_id());

    /// Instruction execution
    csv_writer.write_value(PERF_SAMPLE_IP, sample.instruction_execution().logical_instruction_pointer(), true);
    csv_writer.write_value(PERF_SAMPLE_IP, sample.instruction_execution().physical_instruction_pointer(), true);
    csv_writer.write_value(PERF_SAMPLE_IP, sample.instruction_execution().is_instruction_pointer_exact());
    csv_writer.write_value(PERF_SAMPLE_DATA_SRC, sample.instruction_execution().is_locked());
  }

  file_stream << std::flush;
}