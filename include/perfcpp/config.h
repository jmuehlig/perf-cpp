#pragma once

namespace perf
{
class Config
{
public:
    Config() noexcept = default;
    ~Config() noexcept = default;
    Config(const Config&) noexcept = default;
    Config& operator=(const Config&) noexcept = default;

    [[nodiscard]] std::uint8_t max_groups() const noexcept { return _max_groups; }
    [[nodiscard]] std::uint8_t max_counters_per_group() const noexcept { return _max_counters_per_group; }

    [[nodiscard]] bool is_include_child_threads() const noexcept { return _is_include_child_threads; }
    [[nodiscard]] bool is_include_kernel() const noexcept { return _is_include_kernel; }
    [[nodiscard]] bool is_include_user() const noexcept { return _is_include_user; }
    [[nodiscard]] bool is_include_hypervisor() const noexcept { return _is_include_hypervisor; }
    [[nodiscard]] bool is_include_idle() const noexcept { return _is_include_idle; }
    [[nodiscard]] bool is_include_guest() const noexcept { return _is_include_guest; }

    void max_groups(const std::uint8_t max_groups) noexcept { _max_groups = max_groups; }
    void max_counters_per_group(const std::uint8_t max_counters_per_group) noexcept { _max_counters_per_group = max_counters_per_group; }

    void include_child_threads(const bool is_include_child_threads) noexcept { _is_include_child_threads = is_include_child_threads; }
    void include_kernel(const bool is_include_kernel) noexcept { _is_include_kernel = is_include_kernel; }
    void include_user(const bool is_include_user) noexcept { _is_include_user = is_include_user; }
    void include_hypervisor(const bool is_include_hypervisor) noexcept { _is_include_hypervisor = is_include_hypervisor; }
    void include_idle(const bool is_include_idle) noexcept { _is_include_idle = is_include_idle; }
    void include_guest(const bool is_include_guest) noexcept { _is_include_guest = is_include_guest; }
private:
    std::uint8_t _max_groups { 5U };
    std::uint8_t _max_counters_per_group { 4U };

    bool _is_include_child_threads {false};
    bool _is_include_kernel {true};
    bool _is_include_user {true};
    bool _is_include_hypervisor {true};
    bool _is_include_idle {true};
    bool _is_include_guest {true};
};

class SampleConfig : public Config
{
public:
    [[nodiscard]] std::uint8_t precise_ip() const noexcept { return _precise_ip; }
    [[nodiscard]] std::uint64_t buffer_pages() const noexcept { return _buffer_pages; }
    [[nodiscard]] std::uint64_t frequency_or_period() const noexcept { return _frequency_or_period; }
    [[nodiscard]] bool is_frequency() const noexcept { return _is_frequency; }

    void frequency(const std::uint64_t frequency) noexcept { _is_frequency = true; _frequency_or_period = frequency; }
    void period(const std::uint64_t period) noexcept { _is_frequency = false; _frequency_or_period = period; }
    void precise_ip(const std::uint8_t precise_ip) noexcept { _precise_ip = precise_ip; }
    void buffer_pages(const std::uint64_t buffer_pages) noexcept { _buffer_pages = buffer_pages; }

private:
    std::uint64_t _buffer_pages {8192U + 1U};

    bool _is_frequency;
    std::uint64_t _frequency_or_period;

    std::uint8_t _precise_ip {0};

};
}