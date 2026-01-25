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
    AX = 0U,
    BX = 1U,
    CX = 2U,
    DX = 3U,
    SI = 4U,
    DI = 5U,
    BP = 6U,
    SP = 7U,
    IP = 8U,
    FLAGS = 9U,
    CS = 10U,
    SS = 11U,
    DS = 12U,
    ES = 13U,
    FS = 14U,
    GS = 15U,
    R8 = 16U,
    R9 = 17U,
    R10 = 18U,
    R11 = 19U,
    R12 = 20U,
    R13 = 21U,
    R14 = 22U,
    R15 = 23U,

    XMM0 = 32U,
    XMM1 = 34U,
    XMM2 = 36U,
    XMM3 = 38U,
    XMM4 = 40U,
    XMM5 = 42U,
    XMM6 = 44U,
    XMM7 = 46U,
    XMM8 = 48U,
    XMM9 = 50U,
    XMM10 = 52U,
    XMM11 = 54U,
    XMM12 = 56U,
    XMM13 = 58U,
    XMM14 = 60U,
    XMM15 = 62U
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
    X0 = 0U,
    X1 = 1U,
    X2 = 2U,
    X3 = 3U,
    X4 = 4U,
    X5 = 5U,
    X6 = 6U,
    X7 = 7U,
    X8 = 8U,
    X9 = 9U,
    X10 = 10U,
    X11 = 11U,
    X12 = 12U,
    X13 = 13U,
    X14 = 14U,
    X15 = 15U,
    X16 = 16U,
    X17 = 17U,
    X18 = 18U,
    X19 = 19U,
    X20 = 20U,
    X21 = 21U,
    X22 = 22U,
    X23 = 23U,
    X24 = 24U,
    X25 = 25U,
    X26 = 26U,
    X27 = 27U,
    X28 = 28U,
    X29 = 29U,
    LR = 30U,
    SP = 31U,
    PC = 32U,
    MAX = 33U,
    VG = 46U
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

  [[nodiscard]] std::uint64_t mask() const
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

  [[nodiscard]] std::uint64_t size() const
  {
    return std::visit([](const auto& registers) { return registers.size(); }, _registers);
  }

  [[nodiscard]] bool empty() const
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

  RegisterValues(const RegisterValues&) = default;
  RegisterValues(RegisterValues&&) noexcept = default;

  ~RegisterValues() = default;

  RegisterValues& operator=(const RegisterValues&) = default;
  RegisterValues& operator=(RegisterValues&&) noexcept = default;

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
  ABI _abi{ ABI::None };

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