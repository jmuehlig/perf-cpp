#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <functional>
#include "config.h"
#include "counter.h"
#include "counter_definition.h"

namespace perf
{
class Sampler
{
public:
    enum Type : std::uint64_t
    {
        Identifier = PERF_SAMPLE_IDENTIFIER,
        InstructionPointer = PERF_SAMPLE_IP,
        ThreadId = PERF_SAMPLE_TID,
        CPU = PERF_SAMPLE_CPU,
        Timestamp = PERF_SAMPLE_TIME,
        LogicalMemAddress = PERF_SAMPLE_ADDR,
        PhysicalMemAddress = PERF_SAMPLE_PHYS_ADDR,
    };

    Sampler(const CounterDefinition& counter_list, const std::string& counter_name, const std::uint64_t type, const std::uint64_t frequency, Config config = {})
        : Sampler(counter_list, std::string{counter_name}, type, frequency, config)
    {
    }

    Sampler(const CounterDefinition& counter_list, std::string&& counter_name, std::uint64_t type, std::uint64_t frequency, Config config = {});

    Sampler(Sampler&&) noexcept = default;
    Sampler(const Sampler&) = default;

    ~Sampler() = default;

    /**
     * Opens and starts recording performance counters.
     *
     * @return True, of the performance counters could be started.
     */
    bool start();

    /**
     * Stops recording performance counters.
     */
    void stop();

    /**
     * Closes the sampler, including mapped buffer.
     */
    void close();

    void for_each_sample(std::function<void(void*)> &&callback);

    [[nodiscard]] std::int64_t last_error() const noexcept { return _last_error; }

private:
    const CounterDefinition& _counter_definitions;

    /// Perf config.
    Config _config;

    /// Additional config for sampling.
    std::pair<std::uint64_t, std::uint64_t> _sample_config;

    /// Real counter to measure.
    std::optional<Counter> _counter { std::nullopt };

    /// Buffer for the samples.
    void *_buffer {nullptr};

    /// Will be assigned to errorno.
    std::int64_t _last_error {0};

    [[nodiscard]] bool open();
};
}