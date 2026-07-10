#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include <perfcpp/counter/config.hpp>
#include <perfcpp/event_counter.hpp>
#include <perfcpp/exception.hpp>
#include <perfcpp/sample/config.hpp>
#include <perfcpp/sampler.hpp>
#include <unistd.h>

TEST_CASE("Config defaults", "[Config]")
{
  const auto config = perf::Config{};

  SECTION("boolean defaults")
  {
    REQUIRE_FALSE(config.is_include_child_threads());
    REQUIRE(config.is_include_kernel());
    REQUIRE(config.is_include_user());
    REQUIRE(config.is_include_hypervisor());
    REQUIRE(config.is_include_idle());
    REQUIRE(config.is_include_guest());
    REQUIRE(config.is_include_host());
    REQUIRE_FALSE(config.is_pinned());
    REQUIRE_FALSE(config.is_debug());
  }

  SECTION("cpu core defaults to any")
  {
    REQUIRE(config.cpu_core() == perf::CpuCore::Any);
    REQUIRE(config.cpu_core().is_any());
  }

  SECTION("process defaults to calling")
  {
    REQUIRE(config.process() == perf::Process::Calling);
    REQUIRE(config.process().is_calling());
  }
}

TEST_CASE("Config setter roundtrips", "[Config]")
{
  auto config = perf::Config{};

  SECTION("include_child_threads")
  {
    config.include_child_threads(true);
    REQUIRE(config.is_include_child_threads());
    config.include_child_threads(false);
    REQUIRE_FALSE(config.is_include_child_threads());
  }

  SECTION("include_kernel")
  {
    config.include_kernel(false);
    REQUIRE_FALSE(config.is_include_kernel());
  }

  SECTION("include_user")
  {
    config.include_user(false);
    REQUIRE_FALSE(config.is_include_user());
  }

  SECTION("include_hypervisor")
  {
    config.include_hypervisor(false);
    REQUIRE_FALSE(config.is_include_hypervisor());
  }

  SECTION("include_idle")
  {
    config.include_idle(false);
    REQUIRE_FALSE(config.is_include_idle());
  }

  SECTION("include_guest")
  {
    config.include_guest(false);
    REQUIRE_FALSE(config.is_include_guest());
  }

  SECTION("include_host")
  {
    config.include_host(false);
    REQUIRE_FALSE(config.is_include_host());
  }

  SECTION("pinned")
  {
    config.pinned(true);
    REQUIRE(config.is_pinned());
  }

  SECTION("debug")
  {
    config.debug(true);
    REQUIRE(config.is_debug());
  }

  SECTION("num_physical_counters")
  {
    config.num_physical_counters(2U);
    REQUIRE(config.num_physical_counters() == 2U);
  }

  SECTION("num_events_per_physical_counter")
  {
    config.num_events_per_physical_counter(8U);
    REQUIRE(config.num_events_per_physical_counter() == 8U);
  }

  SECTION("cpu_core from CpuCore")
  {
    config.cpu_core(perf::CpuCore{ std::uint16_t{ 5U } });
    REQUIRE(config.cpu_core() == std::uint16_t{ 5U });
    REQUIRE_FALSE(config.cpu_core().is_any());
  }

  SECTION("cpu_core from integer")
  {
    config.cpu_core(3U);
    REQUIRE(config.cpu_core() == std::uint16_t{ 3U });
  }

  SECTION("cpu_core revert to any")
  {
    config.cpu_core(5U);
    config.cpu_core(perf::CpuCore::Any);
    REQUIRE(config.cpu_core().is_any());
  }

  SECTION("process from Process")
  {
    config.process(perf::Process{ 1337 });
    REQUIRE(config.process() == perf::Process{ 1337 });
    REQUIRE_FALSE(config.process().is_calling());
    REQUIRE_FALSE(config.process().is_any());
  }

  SECTION("process from pid")
  {
    config.process(static_cast<pid_t>(42));
    REQUIRE(config.process() == 42);
  }

  SECTION("process revert to any")
  {
    config.process(perf::Process{ 1337 });
    config.process(perf::Process::Any);
    REQUIRE(config.process().is_any());
  }
}

TEST_CASE("Config with explicit counter limits", "[Config]")
{
  const auto config = perf::Config{ 2U, 1U };

  REQUIRE(config.num_physical_counters() == 2U);
  REQUIRE(config.num_events_per_physical_counter() == 1U);
}

TEST_CASE("Process and CpuCore", "[Config]")
{
  SECTION("Process statics are distinct")
  {
    REQUIRE_FALSE(perf::Process::Any == perf::Process::Calling);
    REQUIRE(perf::Process::Any.is_any());
    REQUIRE_FALSE(perf::Process::Any.is_calling());
    REQUIRE(perf::Process::Calling.is_calling());
    REQUIRE_FALSE(perf::Process::Calling.is_any());
  }

  SECTION("Process from pid")
  {
    const auto process = perf::Process{ 1234 };
    REQUIRE(process == 1234);
    REQUIRE_FALSE(process.is_any());
    REQUIRE_FALSE(process.is_calling());
  }

  SECTION("CpuCore any")
  {
    REQUIRE(perf::CpuCore::Any.is_any());
  }

  SECTION("CpuCore from id")
  {
    const auto core = perf::CpuCore{ std::uint16_t{ 7U } };
    REQUIRE(core == std::uint16_t{ 7U });
    REQUIRE_FALSE(core.is_any());
  }
}

TEST_CASE("InvalidConfigAnyCpuCoreAndAnyProcess", "[Config]")
{
  SECTION("EventCounter throws on open")
  {
    auto config = perf::Config{};
    config.cpu_core(perf::CpuCore::Any);
    config.process(perf::Process::Any);

    auto event_counter = perf::EventCounter{ config };
    event_counter.add("instructions");

    REQUIRE_THROWS_AS(event_counter.open(), perf::InvalidConfigAnyCpuCoreAndAnyProcess);
  }

  SECTION("Sampler throws on open")
  {
    auto config = perf::SampleConfig{};
    config.cpu_core(perf::CpuCore::Any);
    config.process(perf::Process::Any);

    auto sampler = perf::Sampler{ config };
    sampler.trigger("cycles");

    REQUIRE_THROWS_AS(sampler.open(), perf::InvalidConfigAnyCpuCoreAndAnyProcess);
  }
}

TEST_CASE("CGroupMonitor from SharedFileDescriptor", "[CGroupMonitor]")
{
  int fds[2];
  REQUIRE(::pipe(fds) == 0);

  SECTION("move SharedFileDescriptor")
  {
    auto sfd = perf::util::SharedFileDescriptor{ fds[1] };
    const auto monitor = perf::CGroupMonitor{ std::move(sfd) };
    REQUIRE(monitor.is_valid());
    REQUIRE(monitor.file_descriptor() == fds[1]);
    REQUIRE_FALSE(sfd.has_value());
  }

  SECTION("copy SharedFileDescriptor")
  {
    auto sfd = perf::util::SharedFileDescriptor{ fds[1] };
    const auto monitor = perf::CGroupMonitor{ sfd };
    REQUIRE(monitor.is_valid());
    REQUIRE(monitor.file_descriptor() == fds[1]);
    REQUIRE(sfd.has_value()); /// Original still valid.
  }

  ::close(fds[0]);
}

TEST_CASE("CGroupMonitor from UniqueFileDescriptor", "[CGroupMonitor]")
{
  int fds[2];
  REQUIRE(::pipe(fds) == 0);

  auto ufd = perf::util::UniqueFileDescriptor{ fds[1] };
  const auto monitor = perf::CGroupMonitor{ std::move(ufd) };

  REQUIRE(monitor.is_valid());
  REQUIRE(monitor.file_descriptor() == fds[1]);

  /// UniqueFileDescriptor must be emptied after transfer.
  REQUIRE_FALSE(ufd.has_value());

  ::close(fds[0]);
}

TEST_CASE("CGroupMonitor from empty UniqueFileDescriptor is invalid", "[CGroupMonitor]")
{
  /// Promoting an empty (never opened) unique file descriptor must not report a valid cgroup.
  auto ufd = perf::util::UniqueFileDescriptor{};
  const auto monitor = perf::CGroupMonitor{ std::move(ufd) };

  REQUIRE_FALSE(monitor.is_valid());
}

TEST_CASE("CGroupMonitor from path", "[CGroupMonitor]")
{
  SECTION("valid path succeeds")
  {
    const auto monitor = perf::CGroupMonitor{ std::filesystem::path{ "/tmp" } };
    REQUIRE(monitor.is_valid());
    REQUIRE(monitor.file_descriptor() >= 0);
  }

  SECTION("nonexistent path throws CannotOpenCGroupError")
  {
    REQUIRE_THROWS_AS(perf::CGroupMonitor{ std::filesystem::path{ "/nonexistent/cgroup/path" } },
                      perf::CannotOpenCGroupError);
  }
}

TEST_CASE("CGroupMonitor from string", "[CGroupMonitor]")
{
  /// A valid name maps to /sys/fs/cgroup/{name}. Use an invalid name to test the throw path
  /// without requiring a real cgroup to exist.
  REQUIRE_THROWS_AS(perf::CGroupMonitor{ std::string{ "nonexistent-cgroup-xyz" } }, perf::CannotOpenCGroupError);
}

TEST_CASE("CGroupMonitor copy shares file descriptor", "[CGroupMonitor]")
{
  int fds[2];
  REQUIRE(::pipe(fds) == 0);

  auto sfd = perf::util::SharedFileDescriptor{ fds[1] };
  const auto monitor_a = perf::CGroupMonitor{ sfd };
  const auto monitor_b = monitor_a;

  REQUIRE(monitor_a.is_valid());
  REQUIRE(monitor_b.is_valid());
  REQUIRE(monitor_a.file_descriptor() == monitor_b.file_descriptor());

  ::close(fds[0]);
}

TEST_CASE("Config cgroup target", "[Config]")
{
  SECTION("default is not a cgroup")
  {
    const auto config = perf::Config{};
    REQUIRE_FALSE(config.is_cgroup());
  }

  SECTION("set cgroup switches variant")
  {
    int fds[2];
    REQUIRE(::pipe(fds) == 0);

    auto config = perf::Config{};
    config.cgroup(perf::CGroupMonitor{ perf::util::SharedFileDescriptor{ fds[1] } });

    REQUIRE(config.is_cgroup());
    REQUIRE(config.cgroup().is_valid());
    REQUIRE(config.cgroup().file_descriptor() == fds[1]);

    ::close(fds[0]);
  }

  SECTION("switching back to process clears cgroup")
  {
    int fds[2];
    REQUIRE(::pipe(fds) == 0);

    auto config = perf::Config{};
    config.cgroup(perf::CGroupMonitor{ perf::util::SharedFileDescriptor{ fds[1] } });
    REQUIRE(config.is_cgroup());

    config.process(perf::Process::Calling);
    REQUIRE_FALSE(config.is_cgroup());
    REQUIRE(config.process().is_calling());

    ::close(fds[0]);
  }

  SECTION("process() throws when cgroup is active")
  {
    int fds[2];
    REQUIRE(::pipe(fds) == 0);

    auto config = perf::Config{};
    config.cgroup(perf::CGroupMonitor{ perf::util::SharedFileDescriptor{ fds[1] } });

    REQUIRE_THROWS_AS(config.process(), std::bad_variant_access);

    ::close(fds[0]);
  }

  SECTION("Config copy with cgroup shares file descriptor")
  {
    int fds[2];
    REQUIRE(::pipe(fds) == 0);

    auto config_a = perf::Config{};
    config_a.cgroup(perf::CGroupMonitor{ perf::util::SharedFileDescriptor{ fds[1] } });

    const auto config_b = config_a;
    REQUIRE(config_b.is_cgroup());
    REQUIRE(config_b.cgroup().file_descriptor() == fds[1]);

    ::close(fds[0]);
  }
}

TEST_CASE("InvalidConfigCGroupMonitorWithAnyCpuCore", "[Config]")
{
  int fds[2];
  REQUIRE(::pipe(fds) == 0);

  auto config = perf::Config{};
  config.cgroup(perf::CGroupMonitor{ perf::util::SharedFileDescriptor{ fds[1] } });
  /// cpu_core defaults to CpuCore::Any — this must be rejected before perf_event_open.

  SECTION("EventCounter throws on open")
  {
    auto event_counter = perf::EventCounter{ config };
    event_counter.add("instructions");
    REQUIRE_THROWS_AS(event_counter.open(), perf::InvalidConfigCGroupMonitorWithAnyCpuCore);
  }

  SECTION("Sampler throws on open")
  {
    auto sample_config = perf::SampleConfig{};
    sample_config.cgroup(perf::CGroupMonitor{ perf::util::SharedFileDescriptor{ fds[1] } });

    auto sampler = perf::Sampler{ sample_config };
    sampler.trigger("cycles");
    REQUIRE_THROWS_AS(sampler.open(), perf::InvalidConfigCGroupMonitorWithAnyCpuCore);
  }

  ::close(fds[0]);
}

TEST_CASE("SampleConfig defaults", "[SampleConfig]")
{
  const auto config = perf::SampleConfig{};

  SECTION("inherits Config defaults")
  {
    REQUIRE(config.is_include_kernel());
    REQUIRE(config.is_include_user());
    REQUIRE_FALSE(config.is_debug());
  }

  SECTION("buffer pages")
  {
    REQUIRE(config.buffer_pages() == 4097U);
  }

  SECTION("precision")
  {
    REQUIRE(config.precise_ip() == perf::Precision::MustHaveConstantSkid);
  }

  SECTION("clock")
  {
    REQUIRE_FALSE(config.clock().has_value());
  }
}

TEST_CASE("SampleConfig setter roundtrips", "[SampleConfig]")
{
  auto config = perf::SampleConfig{};

  SECTION("period")
  {
    config.period(50000U);
    REQUIRE(std::holds_alternative<perf::Period>(config.period_or_frequency()));
    REQUIRE(std::get<perf::Period>(config.period_or_frequency()).get() == 50000U);
  }

  SECTION("frequency")
  {
    config.frequency(1000U);
    REQUIRE(std::holds_alternative<perf::Frequency>(config.period_or_frequency()));
    REQUIRE(std::get<perf::Frequency>(config.period_or_frequency()).get() == 1000U);
  }

  SECTION("period and frequency are mutually exclusive")
  {
    config.period(50000U);
    config.frequency(1000U);
    REQUIRE(std::holds_alternative<perf::Frequency>(config.period_or_frequency()));
  }

  SECTION("precision")
  {
    config.precision(perf::Precision::RequestZeroSkid);
    REQUIRE(config.precise_ip() == perf::Precision::RequestZeroSkid);
  }

  SECTION("buffer_pages")
  {
    config.buffer_pages(8193U);
    REQUIRE(config.buffer_pages() == 8193U);
  }

  SECTION("clock")
  {
    config.clock(perf::Clock::Monotonic);
    REQUIRE(config.clock().has_value());
    REQUIRE(config.clock().value() == perf::Clock::Monotonic);
  }

  SECTION("clock clear with nullopt")
  {
    config.clock(perf::Clock::Monotonic);
    config.clock(std::nullopt);
    REQUIRE_FALSE(config.clock().has_value());
  }
}
