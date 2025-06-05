#pragma once

#include <bitset>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

namespace perf {
enum class ABI : std::uint8_t
{
  None = 0U,
  Regs32 = 1U,
  Regs64 = 2U
};
class Registers
{
public:
  enum class x86 : std::uint8_t
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

  enum class arm : std::uint8_t
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

  enum class arm64 : std::uint8_t
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

  enum class riscv : std::uint8_t
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

  using registers_t = std::variant<std::vector<x86>, std::vector<arm>, std::vector<arm64>, std::vector<riscv>>;

  Registers() noexcept = default;

  Registers(Registers&&) noexcept = default;
  Registers(const Registers&) = default;

  Registers& operator=(Registers&&) noexcept = default;
  Registers& operator=(const Registers&) = default;

  explicit Registers(std::vector<x86>&& registers) noexcept
    : _registers(std::move(registers))
  {
  }

  explicit Registers(std::vector<arm>&& registers) noexcept
    : _registers(std::move(registers))
  {
  }

  explicit Registers(std::vector<arm64>&& registers) noexcept
    : _registers(std::move(registers))
  {
  }

  explicit Registers(std::vector<riscv>&& registers) noexcept
    : _registers(std::move(registers))
  {
  }

  ~Registers() noexcept = default;

  [[nodiscard]] std::uint64_t mask() const noexcept
  {
    return std::visit(
      [](const auto& registers) {
        auto mask = 0ULL;
        for (const auto reg : registers) {
          mask |= static_cast<std::uint64_t>(1U) << static_cast<std::uint64_t>(reg);
        }
        return mask;
      },
      _registers);
  }

  [[nodiscard]] std::uint64_t size() const noexcept
  {
    return std::visit([](const auto& registers) { return registers.size(); }, _registers);
  }

  [[nodiscard]] bool empty() const noexcept
  {
    return std::visit([](const auto& registers) { return registers.empty(); }, _registers);
  }

  [[nodiscard]] const registers_t& registers() const noexcept { return _registers; }

private:
  registers_t _registers;
};

/**
 * Represents sampled values of general-purpose registers for a specific architecture.
 */
class RegisterValues
{
public:
  RegisterValues(const ABI abi, std::unordered_map<std::uint8_t, std::int64_t>&& register_values) noexcept
    : _abi(abi)
    , _values(std::move(register_values))
  {
  }

  explicit RegisterValues(const ABI abi)
    : _abi(abi)
  {
  }

  ~RegisterValues() = default;

  /**
   * @return The ABI for which these register values are valid.
   */
  [[nodiscard]] ABI abi() const noexcept { return _abi; }

  /**
   * @return The value of the specified x86 register, if available.
   */
  [[nodiscard]] std::optional<std::int64_t> get(const Registers::x86 reg) const noexcept
  {
    return get(static_cast<std::uint8_t>(reg));
  }

  /**
   * @return The value of the specified ARM register, if available.
   */
  [[nodiscard]] std::optional<std::int64_t> get(const Registers::arm reg) const noexcept
  {
    return get(static_cast<std::uint8_t>(reg));
  }

  /**
   * @return The value of the specified ARM64 (AArch64) register, if available.
   */
  [[nodiscard]] std::optional<std::int64_t> get(const Registers::arm64 reg) const noexcept
  {
    return get(static_cast<std::uint8_t>(reg));
  }

  /**
   * @return The value of the specified RISC-V register, if available.
   */
  [[nodiscard]] std::optional<std::int64_t> get(const Registers::riscv reg) const noexcept
  {
    return get(static_cast<std::uint8_t>(reg));
  }

  [[nodiscard]] std::optional<std::int64_t> operator[](const Registers::x86 reg) const noexcept
  {
    return get(static_cast<std::uint8_t>(reg));
  }

  [[nodiscard]] std::optional<std::int64_t> operator[](const Registers::arm reg) const noexcept
  {
    return get(static_cast<std::uint8_t>(reg));
  }

  [[nodiscard]] std::optional<std::int64_t> operator[](const Registers::arm64 reg) const noexcept
  {
    return get(static_cast<std::uint8_t>(reg));
  }

  [[nodiscard]] std::optional<std::int64_t> operator[](const Registers::riscv reg) const noexcept
  {
    return get(static_cast<std::uint8_t>(reg));
  }

private:
  /// ABI of the registers.
  ABI _abi;

  /// Map of Register -> Value.
  std::unordered_map<std::uint8_t, std::int64_t> _values;

  /**
   * @return The value of the register identified by its numeric encoding, if available.
   */
  [[nodiscard]] std::optional<std::int64_t> get(const std::uint8_t reg) const noexcept
  {
    if (const auto value = _values.find(reg); value != _values.end()) {
      return value->second;
    }

    return std::nullopt;
  }
};

}