#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <linux/perf_event.h>
#include <optional>


namespace perf
{
class CounterConfig
{
public:
    CounterConfig(const std::uint32_t type, const std::uint64_t event_id) noexcept
        : _type(type), _event_id(event_id)
    {
    }

    ~CounterConfig() noexcept = default;

    [[nodiscard]] std::uint32_t type() const noexcept { return _type; }
    [[nodiscard]] std::uint64_t event_id() const noexcept { return _event_id; }
private:
    std::uint32_t _type;
    std::uint64_t _event_id;
};

class CounterResult
{
public:
    CounterResult(std::string_view name, const double value) : _name(name), _value(value)
    {
    }

    ~CounterResult() = default;

    [[nodiscard]] std::string_view name() const noexcept { return _name; }
    [[nodiscard]] double value() const noexcept { return _value; }
private:
    std::string_view _name;
    double _value;
};

class Counter
{
public:
    explicit Counter(std::string_view name, CounterConfig config) noexcept : _name(name), _config(config) { }
    Counter(Counter&&) noexcept = default;

    ~Counter() noexcept = default;

    [[nodiscard]] std::string_view name() const noexcept { return _name; }

    [[nodiscard]] std::uint32_t type() const noexcept { return _config.type(); }
    [[nodiscard]] std::uint64_t event_id() const noexcept { return _config.event_id(); }

    [[nodiscard]] perf_event_attr &event_attribute() noexcept { return _event_attribute; }
    [[nodiscard]] std::uint64_t& id() noexcept { return _id; }
    [[nodiscard]] std::uint64_t id() const noexcept { return _id; }

    void file_descriptor(const std::int32_t file_descriptor) noexcept { _file_descriptor = file_descriptor; }
    [[nodiscard]] std::int32_t file_descriptor() const noexcept { return _file_descriptor; }
    [[nodiscard]] bool is_open() const noexcept { return _file_descriptor > -1; }
private:
    std::string_view _name;
    CounterConfig _config;
    perf_event_attr _event_attribute;
    std::uint64_t _id;
    std::int32_t _file_descriptor{-1};
};

class Group
{
public:
    Group() = default;
    ~Group() = default;

    Group(Group&&) noexcept = default;

    constexpr static inline auto MAX_MEMBERS = 4U;
    bool add(Counter&& counter);

     bool open();
    void close();

    bool start();
    bool stop();

    [[nodiscard]] bool is_full() const noexcept { return _members.size() >= MAX_MEMBERS; }

    [[nodiscard]] std::int32_t leader_file_descriptor() const noexcept
    {
        return !_members.empty() ? _members.front().file_descriptor() : -1;
    }

    [[nodiscard]] std::optional<double> get(const std::string& name) const;
    [[nodiscard]] std::optional<double> get(std::string&& name) const { return get(name); }
    [[nodiscard]] std::vector<CounterResult> get() const;
private:
    struct read_format
    {
        struct value
        {
            std::uint64_t value;
            std::uint64_t id;
        };

        std::uint64_t count_members;
        std::uint64_t time_enabled{0U};
        std::uint64_t time_running{0U};
        std::array<value, MAX_MEMBERS> values;
    };

    std::vector<Counter> _members;

    read_format _start_value;

    read_format _end_value;

    [[nodiscard]] static std::optional<std::uint64_t> value_for_id(const read_format& value, const std::uint64_t id) noexcept
    {
        for (auto i = 0U; i < value.count_members; ++i)
        {
            if (value.values[i].id == id)
            {
                return value.values[i].value;
            }
        }

        return std::nullopt;
    }
};
}