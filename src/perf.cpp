#include <perfcpp/perf.h>

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

bool perf::Perf::open()
{
    auto is_all_open = true;
    for (auto& group : this->_groups)
    {
        is_all_open &= group.open();
    }

    return is_all_open;
}

void perf::Perf::close()
{
    for (auto& group : this->_groups)
    {
        group.close();
    }
}

bool perf::Perf::start()
{
    auto is_all_started = true;
    for (auto& group : this->_groups)
    {
        is_all_started &= group.start();
    }

    return is_all_started;
}

bool perf::Perf::stop()
{
    auto is_all_stopped = true;
    for (auto& group : this->_groups)
    {
        is_all_stopped &= group.stop();
    }

    return is_all_stopped;
}

std::optional<double> perf::Perf::get(const std::string &name) const
{
    for (const auto& group : this->_groups)
    {
        const auto result = group.get(name);
        if (result.has_value())
        {
            return result;
        }
    }

    return std::nullopt;
}

std::vector<perf::CounterResult> perf::Perf::get() const
{
    auto results = std::vector<perf::CounterResult>{};
    results.reserve(MAX_GROUPS * perf::Group::MAX_MEMBERS);

    for(const auto& group : this->_groups)
    {
        auto group_results = group.get();
        std::move(group_results.begin(), group_results.end(), std::back_inserter(results));
    }

    return results;
}