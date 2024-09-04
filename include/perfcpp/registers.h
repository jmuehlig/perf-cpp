#pragma once

#include <bitset>
#include <cstdint>
#include <vector>

namespace perf {
class Registers
{
public:
  enum class x86
  {
    AX,
    BX,
    CX,
    DX,
    SI,
    DI,
    BP,
    SP,
    IP,
    FLAGS,
    CS,
    SS,
    DS,
    ES,
    FS,
    GS,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,

    XMM0 = 32,
    XMM1 = 34,
    XMM2 = 36,
    XMM3 = 38,
    XMM4 = 40,
    XMM5 = 42,
    XMM6 = 44,
    XMM7 = 46,
    XMM8 = 48,
    XMM9 = 50,
    XMM10 = 52,
    XMM11 = 54,
    XMM12 = 56,
    XMM13 = 58,
    XMM14 = 60,
    XMM15 = 62
  };

  enum class arm
  {
    R0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    R8,
    R9,
    R10,
    FP,
    IP,
    SP,
    LR,
    PC,
    MAX
  };

  enum class arm64
  {
    X0,
    X1,
    X2,
    X3,
    X4,
    X5,
    X6,
    X7,
    X8,
    X9,
    X10,
    X11,
    X12,
    X13,
    X14,
    X15,
    X16,
    X17,
    X18,
    X19,
    X20,
    X21,
    X22,
    X23,
    X24,
    X25,
    X26,
    X27,
    X28,
    X29,
    LR,
    SP,
    PC,
    MAX,
    VG = 46
  };

  enum class riscv
  {
    PC,
    RA,
    SP,
    GP,
    TP,
    T0,
    T1,
    T2,
    S0,
    S1,
    A0,
    A1,
    A2,
    A3,
    A4,
    A5,
    A6,
    A7,
    S2,
    S3,
    S4,
    S5,
    S6,
    S7,
    S8,
    S9,
    S10,
    S11,
    T3,
    T4,
    T5,
    T6
  };

  Registers() noexcept = default;

  explicit Registers(std::vector<x86>&& registers) noexcept
  {
    for (const auto reg : registers) {
      _mask |= static_cast<std::uint64_t>(1U) << static_cast<std::uint64_t>(reg);
    }
  }

  explicit Registers(std::vector<arm>&& registers) noexcept
  {
    for (const auto reg : registers) {
      _mask |= static_cast<std::uint64_t>(1U) << static_cast<std::uint64_t>(reg);
    }
  }

  explicit Registers(std::vector<arm64>&& registers) noexcept
  {
    for (const auto reg : registers) {
      _mask |= static_cast<std::uint64_t>(1U) << static_cast<std::uint64_t>(reg);
    }
  }

  explicit Registers(std::vector<riscv>&& registers) noexcept
  {
    for (const auto reg : registers) {
      _mask |= static_cast<std::uint64_t>(1U) << static_cast<std::uint64_t>(reg);
    }
  }

  ~Registers() noexcept = default;

  [[nodiscard]] std::uint64_t mask() const noexcept { return _mask; }

  [[nodiscard]] std::uint64_t size() const noexcept { return std::bitset<64>{ _mask }.count(); }

private:
  std::uint64_t _mask{ 0U };
};
}