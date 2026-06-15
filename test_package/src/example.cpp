#include <cstdlib>
#include <iostream>
#include <perfcpp/event_counter.hpp>

int
main()
{
  auto counter_definitions = perf::CounterDefinition{};
  auto event_counter = perf::EventCounter{ counter_definitions };

  event_counter.add("instructions");
  std::cout << "perf-cpp: Successfully configured 'instructions' counter." << std::endl;

  return EXIT_SUCCESS;
}
