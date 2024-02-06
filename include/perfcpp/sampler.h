#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <functional>
#include "config.h"
#include "group.h"
#include "counter_definition.h"

namespace perf
{
class Sampler
{
public:
    /**
     * What to sample.
     */
    enum Type : std::uint64_t
    {
        InstructionPointer = PERF_SAMPLE_IP,
        ThreadId = PERF_SAMPLE_TID,
        Timestamp = PERF_SAMPLE_TIME,
        LogicalMemAddress = PERF_SAMPLE_ADDR,
        CounterValues = PERF_SAMPLE_READ,
        CPU = PERF_SAMPLE_CPU,
        DataSource = PERF_SAMPLE_DATA_SRC,
        Identifier = PERF_SAMPLE_IDENTIFIER,
        PhysicalMemAddress = PERF_SAMPLE_PHYS_ADDR,
    };

    /**
     * Mode a sample was collected.
     */
    enum SampleMode
    {
        Unknown,
        Kernel,
        User,
        Hypervisor,
        GuestKernel,
        GuestUser
    };

    Sampler(const CounterDefinition& counter_list, const std::string& counter_name, const std::uint64_t type, SampleConfig config = {})
        : Sampler(counter_list, std::string{counter_name}, type, config)
    {
    }

    Sampler(const CounterDefinition& counter_list, std::string&& counter_name, const std::uint64_t type, SampleConfig config = {})
        : Sampler(counter_list, std::vector<std::string>{std::move(counter_name)}, type, config)
    {
    }

    Sampler(const CounterDefinition& counter_list, std::vector<std::string>&& counter_names, std::uint64_t type, SampleConfig config = {});

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

    void for_each_sample(std::function<void(void*, SampleMode)> &&callback);

    [[nodiscard]] std::int64_t last_error() const noexcept { return _last_error; }

private:
    const CounterDefinition& _counter_definitions;

    /// Perf config.
    SampleConfig _config;

    std::uint64_t _sample_type;

    /// Real counter to measure.
    class Group _group;

    /// Buffer for the samples.
    void *_buffer {nullptr};

    /// Will be assigned to errorno.
    std::int64_t _last_error {0};

    [[nodiscard]] bool open();
};
}