#pragma once

#include "counter.h"
#include "config.h"

namespace perf
{
    class Group
    {
    public:
        Group() = default;
        ~Group() = default;

        Group(Group&&) noexcept = default;
        Group(const Group&) = default;

        constexpr static inline auto MAX_MEMBERS = 8U;
        bool add(CounterConfig counter);

        bool open(Config config);
        void close();

        bool start();
        bool stop();

        [[nodiscard]] std::size_t size() const noexcept { return _members.size(); }

        [[nodiscard]] std::int32_t leader_file_descriptor() const noexcept
        {
            return !_members.empty() ? _members.front().file_descriptor() : -1;
        }

        [[nodiscard]] double get(std::size_t index) const;
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