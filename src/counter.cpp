#include <perfcpp/counter.h>
#include <algorithm>
#include <sstream>

std::optional<double> perf::CounterResult::get(const std::string& name) const noexcept
{
    if (auto iterator = std::find_if(this->_results.begin(), this->_results.end(), [&name](const auto res) { return name == res.first; }); iterator != this->_results.end())
    {
        return iterator->second;
    }

    return std::nullopt;
}

std::string perf::CounterResult::to_json() const
{
    auto json_stream = std::stringstream{};

    json_stream << "{";

    for (auto i = 0U; i < this->_results.size(); ++i)
    {
        if (i > 0U)
        {
            json_stream << ",";
        }

        json_stream << "\"" << this->_results[i].first << "\": " << this->_results[i].second;
    }

    json_stream << "}";

    return json_stream.str();
}

std::string perf::CounterResult::to_csv(const char delimiter, const bool print_header) const
{
    auto csv_stream = std::stringstream{};

    if (print_header)
    {
        csv_stream << "counter" << delimiter << "value\n";
    }

    for (auto i = 0U; i < this->_results.size(); ++i)
    {
        if (i > 0U)
        {
            csv_stream << "\n";
        }

        csv_stream << this->_results[i].first << delimiter << this->_results[i].second;
    }

    return csv_stream.str();
}