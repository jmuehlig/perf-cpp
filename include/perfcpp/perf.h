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

    bool add(std::string&& counter_name) { return add(counter_name); }
    bool add(const std::string& counter_name);
    bool add(std::vector<std::string>&& counter_names) { return add(counter_names); }
    bool add(const std::vector<std::string>& counter_names);

    bool open();
    void close();

    bool start();
    bool stop();

    [[nodiscard]] std::optional<double> get(const std::string& name) const;
    [[nodiscard]] std::optional<double> get(std::string&& name) const { return get(name); }
    [[nodiscard]] std::vector<CounterResult> get() const;

private:
    const CounterDefinition& _counter_definitions;

    std::vector<Group> _groups;
};
}