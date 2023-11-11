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
     * Opens all the performance counters.
     * However, they will not start counting.
     *
     * @return True, if the counters could be opened.
     */
    bool open();

    /**
     * Closes all performance counters.
     * The values can be read afterwards.
     */
    void close();

    /**
     * Starts recording performance counters.
     *
     * @return True, of the performance counters could be started.
     */
    bool start();

    /**
     * Stops recording performance counters.
     *
     * @return True, of the performance counters could be stopped.
     */
    bool stop();

    /**
     * Reads the value of a given counter. The counter must be added before recording.
     *
     * @param counter_name Name of the counter.
     * @return Value of the counter, if the counter was found. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<double> get(const std::string& counter_name) const;

    /**
     * Reads the value of a given counter. The counter must be added before recording.
     *
     * @param counter_name Name of the counter.
     * @return Value of the counter, if the counter was found. std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<double> get(std::string&& counter_name) const { return get(counter_name); }

    /**
     * @return Returns a list with all counters and their values.
     */
    [[nodiscard]] std::vector<CounterResult> get() const;

private:
    const CounterDefinition& _counter_definitions;

    std::vector<Group> _groups;
};

class MonitoredSection
{
public:
    explicit MonitoredSection(Perf& perf) : _perf(perf)
    {
        if (_perf.open())
        {
            _perf.start();
        }
    }

    ~MonitoredSection()
    {
        _perf.stop();
        _perf.close();
    }
private:
    Perf& _perf;
};
}