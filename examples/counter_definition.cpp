#include <perfcpp/counter_definition.h>
#include <iostream>

int
main()
{
  std::cout << "libperf-cpp example: This example prints all automatically read events stored in the perf::CounterDefinition.\n"
            << std::endl;

  /// Create custom instance of the counter definition.
  /// If -DINCLUDE_PROCESSOR_EVENTS=1 is set, a source file with hardware-specific events is generated; include that to print all events for this platform.
#ifdef PERFCPP_HAS_PROCESSOR_SPECIFIC_EVENTS
  const auto counter_definition = perf::CounterDefinition{
    std::make_unique<perf::ProcessorSpecificEventProvider>()
  };
#else
  const auto counter_definition = perf::CounterDefinition{};
#endif

  /// Dump to the console without adding further events.
  std::cout << counter_definition.to_string() << std::endl;

  return 0;
}