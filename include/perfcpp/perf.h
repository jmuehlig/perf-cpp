#pragma once

#include <string>
#include <vector>
#include <optional>
#include "counter.h"
#include "counter_definition.h"

namespace perf
{
class Perf
{
private:
    constexpr static inline auto MAX_GROUPS = 5U;
public:
    explicit Perf(const CounterDefinition& counter_list) : _counter_definitions(counter_list) { }
    Perf(Perf&&) noexcept = default;
    Perf(const Perf&) = default;

    /**
     * Add the specified counter to the list of monitored performance counters.
     * The counter must exist within the counter definitions.
     *
     * @param counter_name Name of the counter.
     * @return True, if the counter could be added.
     */
    bool add(std::string&& counter_name) { return add(counter_name); }

    /**
     * Add the specified counter to the list of monitored performance counters.
     * The counter must exist within the counter definitions.
     *
     * @param counter_name Name of the counter.
     * @return True, if the counter could be added.
     */
    bool add(const std::string& counter_name);

    /**
     * Add the specified counters to the list of monitored performance counters.
     * The counters must exist within the counter definitions.
     *
     * @param counter_names List of names of the counters.
     * @return True, if the counters could be added.
     */
    bool add(std::vector<std::string>&& counter_names) { return add(counter_names); }

    /**
     * Add the specified counters to the list of monitored performance counters.
     * The counters must exist within the counter definitions.
     *
     * @param counter_names List of names of the counters.
     * @return True, if the counters could be added.
     */
    bool add(const std::vector<std::string>& counter_names);

    /**
     * Opens and starts recording performance counters.
     *
     * @return True, of the performance counters could be started.
     */
    bool start();

    /**
     * Stops and closes recording performance counters.
     */
    void stop();

    /**
     * @return The result of all counters.
     */
    [[nodiscard]] CounterResult result() const;

    /**
     * Concatenates the results of multiple perf instances,
     * aiming to aggregate the results for measurements of
     * multiple threads.
     *
     * @param instances List of perf instances.
     * @return An aggregated result of all instances.
     */
    [[nodiscard]] static CounterResult aggregate(const std::vector<Perf>& instances);
private:
    const CounterDefinition& _counter_definitions;

    std::vector<Group> _groups;
};
}