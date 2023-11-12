#include <perfcpp/counter.h>
#include <cstring>
#include <string>
#include <algorithm>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/ioctl.h>

perf::CounterResult perf::CounterResult::concat(std::vector<CounterResult> &&results)
{
    if (results.empty())
    {
        return CounterResult{};
    }

    auto result = std::move(results.front());
    for (auto i = 1U; i < results.size(); ++i)
    {
        result += std::move(results[i]);
    }

    return result;
}

std::optional<double> perf::CounterResult::get(const std::string &name) const noexcept
{
    if (auto iterator = std::find_if(this->_results.begin(), this->_results.end(), [&name](const auto res) { return name == res.first; }); iterator != this->_results.end())
    {
        return iterator->second;
    }

    return std::nullopt;
}

perf::CounterResult &perf::CounterResult::operator+=(perf::CounterResult &&other)
{
    for (auto&& result : other._results)
    {
        if (auto iterator = std::find_if(this->_results.begin(), this->_results.end(), [name = result.first](const auto res) { return name == res.first; }); iterator != this->_results.end())
        {
            iterator->second += result.second;
        }
        else
        {
            this->_results.emplace_back(std::move(result));
        }
    }

    return *this;
}

perf::CounterResult &perf::CounterResult::operator/=(const std::uint64_t divisor) noexcept
{
    for (auto& result : _results)
    {
        result.second /= divisor;
    }

    return *this;
}

perf::CounterResult perf::CounterResult::operator/(const std::uint64_t divisor)
{
    auto results = std::vector<std::pair<std::string_view, double>>{};
    std::transform(this->_results.begin(), this->_results.end(), std::back_inserter(results), [divisor](const auto& result) {
        return std::make_pair(result.first, result.second / divisor);
    });

    return CounterResult{std::move(results)};
}

bool perf::Group::open()
{
    /// File descriptor of the group leader.
    auto leader_file_descriptor = std::int32_t{-1};

    auto is_all_open = true;

    for (auto& counter : this->_members)
    {
        /// The first counter will become the leader.
        const auto is_leader = leader_file_descriptor == -1;

        /// Initialize the event.
        auto& perf_event = counter.event_attribute();
        std::memset(&perf_event, 0, sizeof(perf_event_attr));
        perf_event.type = counter.type();
        perf_event.size = sizeof(perf_event_attr);
        perf_event.config = counter.event_id();
        perf_event.disabled = is_leader;

        if (is_leader)
        {
            perf_event.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_GROUP | PERF_FORMAT_ID;
        }
        else
        {
            perf_event.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
        }

        /// Open the counter.
        const std::int32_t file_descriptor = syscall(__NR_perf_event_open, &perf_event, 0, -1, leader_file_descriptor, 0);
        counter.file_descriptor(file_descriptor);
        if (counter.is_open())
        {
            ::ioctl(file_descriptor, PERF_EVENT_IOC_ID, &counter.id());
        }

        /// Set the leader file descriptor.
        if (is_leader)
        {
            leader_file_descriptor = file_descriptor;
        }

        is_all_open &= counter.is_open();
    }

    return is_all_open;
}

void perf::Group::close()
{
    for (auto& counter : this->_members)
    {
        if (counter.is_open())
        {
            ::close(counter.file_descriptor());
            counter.file_descriptor(-1);
        }
    }
}

bool perf::Group::start()
{
    if (this->_members.empty())
    {
        return false;
    }

    const auto leader_file_descriptor = this->leader_file_descriptor();

    ::ioctl(leader_file_descriptor, PERF_EVENT_IOC_RESET, 0);
    ::ioctl(leader_file_descriptor, PERF_EVENT_IOC_ENABLE, 0);

    const auto read_size = ::read(leader_file_descriptor, &this->_start_value, sizeof(read_format));
    return read_size > 0U;

}

bool perf::Group::stop()
{
    if (this->_members.empty())
    {
        return false;
    }

    const auto leader_file_descriptor = this->leader_file_descriptor();

    const auto read_size = ::read(leader_file_descriptor, &this->_end_value, sizeof(read_format));
    ioctl(leader_file_descriptor, PERF_EVENT_IOC_DISABLE, 0);
    return read_size > 0U;
}

bool perf::Group::add(perf::Counter counter)
{
    if (this->is_full())
    {
        return false;
    }

    this->_members.push_back(counter);
    return true;
}

perf::CounterResult perf::Group::result() const
{
    auto results = std::vector<std::pair<std::string_view, double>>{};
    results.reserve(this->_members.size());

    const auto multiplexing_correction = double(this->_end_value.time_enabled - this->_start_value.time_enabled) /
                                         double(this->_end_value.time_running - this->_start_value.time_running);

    for (const auto& counter : this->_members)
    {
        const auto start_value = Group::value_for_id(this->_start_value, counter.id());
        const auto end_value = Group::value_for_id(this->_end_value, counter.id());

        if (start_value.has_value() && end_value.has_value())
        {
            const auto result = double(end_value.value() - start_value.value());
            const auto corrected_result = result > .0 ? result * multiplexing_correction : .0;
            results.emplace_back(counter.name(), corrected_result);
        }
    }

    return CounterResult{std::move(results)};
}