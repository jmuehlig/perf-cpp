#include <perfcpp/perf.h>
#include <algorithm>

bool perf::Perf::add(const std::string &counter_name)
{
    auto counter = this->_counter_definitions.get(counter_name);
    if (!counter.has_value())
    {
        return false;
    }

    if (this->_groups.size() == Perf::MAX_GROUPS && this->_groups.back().is_full())
    {
        return false;
    }

    if (this->_groups.empty() || this->_groups.back().is_full())
    {
        this->_groups.emplace_back();
    }

    return this->_groups.back().add(std::move(counter.value()));
}

bool perf::Perf::add(const std::vector<std::string> &counter_names)
{
    auto is_all_added = true;

    for (const auto& name : counter_names)
    {
        is_all_added &= this->add(name);
    }

    return is_all_added;
}

bool perf::Perf::start()
{
    auto is_every_counter_started = true;

    /// Open the counters.
    for (auto& group : this->_groups)
    {
        is_every_counter_started &= group.open();
    }

    /// Start the counters.
    if (is_every_counter_started)
    {
        for (auto& group : this->_groups)
        {
            is_every_counter_started &= group.start();
        }
    }

    return is_every_counter_started;
}

void perf::Perf::stop()
{
    /// Stop the counters.
    for (auto& group : this->_groups)
    {
        std::ignore = group.stop();
    }


    /// Close the counters.
    for (auto& group : this->_groups)
    {
        group.close();
    }
}

perf::CounterResult perf::Perf::result() const
{
    auto result = CounterResult{};

    for(const auto& group : this->_groups)
    {
        result += group.result();
    }

    return result;
}

perf::CounterResult perf::Perf::aggregate(const std::vector<Perf> &instances)
{
    auto result = CounterResult{};

    for(const auto& perf : instances)
    {
        result += perf.result();
    }

    return result;
}