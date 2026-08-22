#include <array>
#include <catch2/catch_test_macros.hpp>
#include <fcntl.h>
#include <perfcpp/util/shared_file_descriptor.hpp>
#include <perfcpp/util/unique_file_descriptor.hpp>
#include <unistd.h>

/// Returns true if the file descriptor is still open.
static bool
is_fd_open(const int fd)
{
  return ::fcntl(fd, F_GETFD) != -1;
}

/// Opens a pipe and returns {read_end, write_end}. Aborts on failure.
static std::pair<int, int>
make_pipe()
{
  std::array<std::int32_t, 2U> fds{ 0 };
  REQUIRE(::pipe(fds.data()) == 0);
  return { fds[0], fds[1] };
}

TEST_CASE("UniqueFileDescriptor default construction", "[UniqueFileDescriptor]")
{
  const auto ufd = perf::util::UniqueFileDescriptor{};
  REQUIRE_FALSE(ufd.has_value());
}

TEST_CASE("UniqueFileDescriptor construction from fd", "[UniqueFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  {
    const auto ufd = perf::util::UniqueFileDescriptor{ write_fd };
    REQUIRE(ufd.has_value());
    REQUIRE(ufd.value() == write_fd);
    REQUIRE(is_fd_open(write_fd));
  }

  /// Destructor must have closed the fd.
  REQUIRE_FALSE(is_fd_open(write_fd));
  ::close(read_fd);
}

TEST_CASE("UniqueFileDescriptor move construction", "[UniqueFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  auto ufd_a = perf::util::UniqueFileDescriptor{ write_fd };
  auto ufd_b = std::move(ufd_a);

  REQUIRE_FALSE(ufd_a.has_value());
  REQUIRE(ufd_b.has_value());
  REQUIRE(ufd_b.value() == write_fd);
  REQUIRE(is_fd_open(write_fd));

  /// ufd_b destructor closes write_fd.
  ufd_b = perf::util::UniqueFileDescriptor{};
  REQUIRE_FALSE(is_fd_open(write_fd));
  ::close(read_fd);
}

TEST_CASE("UniqueFileDescriptor move assignment", "[UniqueFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  auto ufd_a = perf::util::UniqueFileDescriptor{ write_fd };
  auto ufd_b = perf::util::UniqueFileDescriptor{};
  ufd_b = std::move(ufd_a);

  REQUIRE_FALSE(ufd_a.has_value());
  REQUIRE(ufd_b.has_value());
  REQUIRE(ufd_b.value() == write_fd);

  ufd_b = perf::util::UniqueFileDescriptor{};
  REQUIRE_FALSE(is_fd_open(write_fd));
  ::close(read_fd);
}

TEST_CASE("UniqueFileDescriptor reset closes fd", "[UniqueFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  auto ufd = perf::util::UniqueFileDescriptor{ write_fd };
  REQUIRE(ufd.has_value());

  ufd.reset();
  REQUIRE_FALSE(ufd.has_value());
  REQUIRE_FALSE(is_fd_open(write_fd));

  ::close(read_fd);
}

TEST_CASE("UniqueFileDescriptor release does not close fd", "[UniqueFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  auto ufd = perf::util::UniqueFileDescriptor{ write_fd };
  REQUIRE(ufd.has_value());

  /// release() transfers ownership without closing (used when handing the fd to another owner).
  const auto raw = ufd.release();
  REQUIRE_FALSE(ufd.has_value());
  REQUIRE(raw == write_fd);
  REQUIRE(is_fd_open(write_fd));

  ::close(write_fd);
  ::close(read_fd);
}

TEST_CASE("SharedFileDescriptor default construction", "[SharedFileDescriptor]")
{
  const auto sfd = perf::util::SharedFileDescriptor{};
  REQUIRE_FALSE(sfd.has_value());
}

TEST_CASE("SharedFileDescriptor construction from fd", "[SharedFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  {
    const auto sfd = perf::util::SharedFileDescriptor{ write_fd };
    REQUIRE(sfd.has_value());
    REQUIRE(sfd.value() == write_fd);
    REQUIRE(is_fd_open(write_fd));
  }

  /// Last owner destroyed → fd must be closed.
  REQUIRE_FALSE(is_fd_open(write_fd));
  ::close(read_fd);
}

TEST_CASE("SharedFileDescriptor construction from invalid fd stays empty", "[SharedFileDescriptor]")
{
  /// Invalid (negative) file descriptors must not be taken into ownership, e.g., when passing the
  /// result of a failed ::open() directly.
  const auto sfd = perf::util::SharedFileDescriptor{ -1 };
  REQUIRE_FALSE(sfd.has_value());
  REQUIRE(sfd.value() == -1);

  const auto sfd_negative = perf::util::SharedFileDescriptor{ -42 };
  REQUIRE_FALSE(sfd_negative.has_value());
  REQUIRE(sfd_negative.value() == -1);
}

TEST_CASE("SharedFileDescriptor copy shares ownership", "[SharedFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  {
    auto sfd_a = perf::util::SharedFileDescriptor{ write_fd };
    {
      auto sfd_b = sfd_a; /// Copy: ref count → 2.
      REQUIRE(sfd_b.has_value());
      REQUIRE(sfd_b.value() == write_fd);

      /// sfd_b goes out of scope: ref count → 1, fd still open.
    }
    REQUIRE(is_fd_open(write_fd));

    /// sfd_a goes out of scope: ref count → 0, fd closed.
  }
  REQUIRE_FALSE(is_fd_open(write_fd));
  ::close(read_fd);
}

TEST_CASE("SharedFileDescriptor copy assignment shares ownership", "[SharedFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  auto sfd_a = perf::util::SharedFileDescriptor{ write_fd };
  auto sfd_b = perf::util::SharedFileDescriptor{};
  sfd_b = sfd_a; /// Copy assignment: ref count → 2.

  REQUIRE(sfd_b.has_value());
  REQUIRE(sfd_b.value() == write_fd);

  sfd_a = perf::util::SharedFileDescriptor{}; /// Drop sfd_a: ref count → 1.
  REQUIRE(is_fd_open(write_fd));

  sfd_b = perf::util::SharedFileDescriptor{}; /// Drop sfd_b: ref count → 0, fd closed.
  REQUIRE_FALSE(is_fd_open(write_fd));
  ::close(read_fd);
}

TEST_CASE("SharedFileDescriptor move construction", "[SharedFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  auto sfd_a = perf::util::SharedFileDescriptor{ write_fd };
  auto sfd_b = std::move(sfd_a);

  REQUIRE_FALSE(sfd_a.has_value());
  REQUIRE(sfd_b.has_value());
  REQUIRE(sfd_b.value() == write_fd);
  REQUIRE(is_fd_open(write_fd));

  sfd_b = perf::util::SharedFileDescriptor{};
  REQUIRE_FALSE(is_fd_open(write_fd));
  ::close(read_fd);
}

TEST_CASE("SharedFileDescriptor move assignment", "[SharedFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  auto sfd_a = perf::util::SharedFileDescriptor{ write_fd };
  auto sfd_b = perf::util::SharedFileDescriptor{};
  sfd_b = std::move(sfd_a);

  REQUIRE_FALSE(sfd_a.has_value());
  REQUIRE(sfd_b.has_value());
  REQUIRE(sfd_b.value() == write_fd);

  sfd_b = perf::util::SharedFileDescriptor{};
  REQUIRE_FALSE(is_fd_open(write_fd));
  ::close(read_fd);
}

TEST_CASE("SharedFileDescriptor multiple copies all share the same fd", "[SharedFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  auto sfd_a = perf::util::SharedFileDescriptor{ write_fd };
  auto sfd_b = sfd_a;
  auto sfd_c = sfd_b;

  REQUIRE(sfd_a.value() == write_fd);
  REQUIRE(sfd_b.value() == write_fd);
  REQUIRE(sfd_c.value() == write_fd);

  sfd_a = perf::util::SharedFileDescriptor{};
  REQUIRE(is_fd_open(write_fd));

  sfd_b = perf::util::SharedFileDescriptor{};
  REQUIRE(is_fd_open(write_fd));

  sfd_c = perf::util::SharedFileDescriptor{};
  REQUIRE_FALSE(is_fd_open(write_fd));
  ::close(read_fd);
}

TEST_CASE("SharedFileDescriptor self copy assignment is safe", "[SharedFileDescriptor]")
{
  auto [read_fd, write_fd] = make_pipe();

  auto sfd = perf::util::SharedFileDescriptor{ write_fd };
  auto* self = &sfd;
  sfd = *self; /// Routed through pointer to suppress -Wself-assign-overloaded.
  REQUIRE(sfd.has_value());
  REQUIRE(is_fd_open(write_fd));

  sfd = perf::util::SharedFileDescriptor{};
  REQUIRE_FALSE(is_fd_open(write_fd));
  ::close(read_fd);
}
