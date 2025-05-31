## Single-threaded
add_executable(single-thread EXCLUDE_FROM_ALL examples/single_thread.cpp examples/access_benchmark.cpp)
target_link_libraries(single-thread perf-cpp)

## Multi-threaded; but inherit counter from main-thread
add_executable(inherit-thread EXCLUDE_FROM_ALL examples/inherit_thread.cpp examples/access_benchmark.cpp)
target_link_libraries(inherit-thread perf-cpp)

## Multi-threaded with thread-local counter
add_executable(multi-thread EXCLUDE_FROM_ALL examples/multi_thread.cpp examples/access_benchmark.cpp)
target_link_libraries(multi-thread perf-cpp)

## Multi-CPU with per-CPU counter
add_executable(multi-cpu EXCLUDE_FROM_ALL examples/multi_cpu.cpp examples/access_benchmark.cpp)
target_link_libraries(multi-cpu perf-cpp)

## Multi-Process with per-process counter
add_executable(multi-process EXCLUDE_FROM_ALL examples/multi_process.cpp examples/access_benchmark.cpp)
target_link_libraries(multi-process perf-cpp)

## Sampling instruction pointers
add_executable(instruction-pointer-sampling EXCLUDE_FROM_ALL examples/instruction_pointer_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(instruction-pointer-sampling perf-cpp)

## Sampling instruction pointers
add_executable(counter-sampling EXCLUDE_FROM_ALL examples/counter_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(counter-sampling perf-cpp)

## Branch sampling
add_executable(branch-sampling EXCLUDE_FROM_ALL examples/branch_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(branch-sampling perf-cpp)

## Memory address sampling
add_executable(address-sampling EXCLUDE_FROM_ALL examples/address_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(address-sampling perf-cpp)

## Sampling user_registers
add_executable(register-sampling EXCLUDE_FROM_ALL examples/register_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(register-sampling perf-cpp)

## Sampling on multiple threads
add_executable(multi-thread-sampling EXCLUDE_FROM_ALL examples/multi_thread_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(multi-thread-sampling perf-cpp)

## Sampling on multiple threads
add_executable(multi-cpu-sampling EXCLUDE_FROM_ALL examples/multi_cpu_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(multi-cpu-sampling perf-cpp)

## Sampling with multiple events
add_executable(multi-event-sampling EXCLUDE_FROM_ALL examples/multi_event_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(multi-event-sampling perf-cpp)

## Sampling with raw values
add_executable(context-switch-sampling EXCLUDE_FROM_ALL examples/context_switch_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(context-switch-sampling perf-cpp)

## Analyze Samples with DataAnalyzer
add_executable(memory-access-analyzer EXCLUDE_FROM_ALL examples/memory_access_analyzer.cpp examples/access_benchmark.cpp)
target_link_libraries(memory-access-analyzer perf-cpp)

## Live Events
add_executable(live-events EXCLUDE_FROM_ALL examples/live_events.cpp examples/access_benchmark.cpp)
target_link_libraries(live-events perf-cpp)

## Flame graph
add_executable(flame-graph EXCLUDE_FROM_ALL examples/flame_graph.cpp examples/access_benchmark.cpp)
target_compile_options(flame-graph PUBLIC "-g3")
target_link_libraries(flame-graph perf-cpp)

## One target for all examples
add_custom_target(examples)
add_dependencies(examples
        single-thread inherit-thread multi-thread multi-cpu multi-process
        instruction-pointer-sampling counter-sampling branch-sampling
        address-sampling register-sampling multi-thread-sampling multi-cpu-sampling
        multi-event-sampling context-switch-sampling
        memory-access-analyzer live-events flame-graph)