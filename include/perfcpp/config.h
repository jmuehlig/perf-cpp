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

    void max_groups(const std::uint8_t max_groups) noexcept { _max_groups = max_groups; }
    void max_counters_per_group(const std::uint8_t max_counters_per_group) noexcept { _max_counters_per_group = max_counters_per_group; }

    void include_child_threads(const bool is_include_child_threads) noexcept { _is_include_child_threads = is_include_child_threads; }
    void include_kernel(const bool is_include_kernel) noexcept { _is_include_kernel = is_include_kernel; }
    void include_user(const bool is_include_user) noexcept { _is_include_user = is_include_user; }
    void include_hypervisor(const bool is_include_hypervisor) noexcept { _is_include_hypervisor = is_include_hypervisor; }
    void include_idle(const bool is_include_idle) noexcept { _is_include_idle = is_include_idle; }

private:
    std::uint8_t _max_groups { 5U };
    std::uint8_t _max_counters_per_group { 4U };

    bool _is_include_child_threads {false};
    bool _is_include_kernel {true};
    bool _is_include_user {true};
    bool _is_include_hypervisor {true};
    bool _is_include_idle {true};
};
}