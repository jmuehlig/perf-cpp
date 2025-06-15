#include <perfcpp/counter_definition.h>
#include <iostream>

int
main()
{
  std::cout << "libperf-cpp example: This example prints all automatically read events stored in the perf::CounterDefinition.\n"
            << std::endl;

  /// Create custom instance of the counter definition.
  const auto counter_definition = perf::CounterDefinition{"events/x86/amdzen4.csv"};

  /// Dump to the console without adding further events.
  std::cout << counter_definition.to_string() << std::endl;

  return 0;
}