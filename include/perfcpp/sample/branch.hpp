#pragma once

#include <cstdint>
#include <linux/perf_event.h>
#include <optional>
#include <perfcpp/feature.h>

namespace perf {
/**
 * Branch types that can be filtered when recording branches via sampling.
 * Note: uint32_t is required since PERF_SAMPLE_BRANCH_TYPE_SAVE occupies bit 16.
 */
#if defined(PERFCPP_NO_SAMPLE_BRANCH_CALL) || defined(PERFCPP_NO_SAMPLE_BRANCH_IND_JUMP)
enum class BranchType : std::uint64_t
#else
enum class BranchType : std::uint32_t
#endif
{
  None = 0ULL,

  User = PERF_SAMPLE_BRANCH_USER,
  Kernel = PERF_SAMPLE_BRANCH_KERNEL,
  HyperVisor = PERF_SAMPLE_BRANCH_HV,

  Any = PERF_SAMPLE_BRANCH_ANY,
#ifndef PERFCPP_NO_SAMPLE_BRANCH_CALL
  Call = PERF_SAMPLE_BRANCH_ANY_CALL,
#else
  Call = 1ULL << 61,
#endif
#ifndef PERFCPP_NO_SAMPLE_BRANCH_CALL
  DirectCall = PERF_SAMPLE_BRANCH_CALL,
#else
  DirectCall = 1ULL << 62,
#endif
  IndirectCall = PERF_SAMPLE_BRANCH_IND_CALL,
  Return = PERF_SAMPLE_BRANCH_ANY_RETURN,
#ifndef PERFCPP_NO_SAMPLE_BRANCH_IND_JUMP
  IndirectJump = PERF_SAMPLE_BRANCH_IND_JUMP,
#else
  IndirectJump = 1ULL << 63,
#endif
  Conditional = PERF_SAMPLE_BRANCH_COND,
  TransactionalMemoryAbort = PERF_SAMPLE_BRANCH_ABORT_TX,
  InTransaction = PERF_SAMPLE_BRANCH_IN_TX,
  NotInTransaction = PERF_SAMPLE_BRANCH_NO_TX,
#ifndef PERFCPP_NO_BRANCH_ENTRY_TYPE /// Saving the branch type per entry is supported since Linux 4.15
  TypeSave = PERF_SAMPLE_BRANCH_TYPE_SAVE,
#endif
};

/**
 * A Branch represents one branch from the branch stack, including information where the branch started (and in case of
 * jmp/call where the branch ended), if the branch was predicted correctly, and how long
 */
class Branch
{
public:
  /**
   * Speculation outcome of the branch, as reported by the hardware.
   * Values correspond to perf_branch_spec (PERF_BR_SPEC_*) in linux/perf_event.h.
   */
  enum class Speculation : std::uint8_t
  {
    Wrong = 1,
    Correct = 2,
    SpeculativeCorrect = 3,
  };

  /**
   * Hardware classification of the branch instruction, as reported via PERF_SAMPLE_BRANCH_TYPE_SAVE.
   * Values correspond to perf_branch_type (PERF_BR_*) in linux/perf_event.h.
   */
  enum class Classification : std::uint8_t
  {
    Unknown = 0,
    Conditional = 1,
    Unconditional = 2,
    Indirect = 3,
    Call = 4,
    IndirectCall = 5,
    Return = 6,
    Syscall = 7,
    SyscallReturn = 8,
    ConditionalCall = 9,
    ConditionalReturn = 10,
    ExceptionReturn = 11,
    Interrupt = 12,
    SystemError = 13,
    NotInTransaction = 14,
  };

  Branch(const std::uintptr_t instruction_pointer_from,
         const std::uintptr_t instruction_pointer_to,
         const bool is_mispredicted,
         const bool is_predicted,
         const bool is_in_transaction,
         const bool is_transaction_abort,
         const std::optional<std::uint16_t> cycles,
         const std::optional<Classification> classification,
         const std::optional<Speculation> speculation_result) noexcept
    : _instruction_pointer_from(instruction_pointer_from)
    , _instruction_pointer_to(instruction_pointer_to)
    , _is_mispredicted(is_mispredicted)
    , _is_predicted(is_predicted)
    , _is_in_transaction(is_in_transaction)
    , _is_transaction_abort(is_transaction_abort)
    , _cycles(cycles)
    , _classification(classification)
    , _speculation_result(speculation_result)
  {
  }

  /**
   * @return The instruction pointer the branch started.
   */
  [[nodiscard]] std::uintptr_t instruction_pointer_from() const noexcept { return _instruction_pointer_from; }

  /**
   * @return The instruction pointer the branch ended.
   */
  [[nodiscard]] std::uintptr_t instruction_pointer_to() const noexcept { return _instruction_pointer_to; }

  /**
   * @return True, if the branch was not predicted properly.
   */
  [[nodiscard]] bool is_mispredicted() const noexcept { return _is_mispredicted; }

  /**
   * @return True, if the branch was predicted correctly.
   */
  [[nodiscard]] bool is_predicted() const noexcept { return _is_predicted; }

  /**
   * @return True, if the branch was within a memory transaction.
   */
  [[nodiscard]] bool is_in_transaction() const noexcept { return _is_in_transaction; }

  /**
   * @return True, if the branch was a transaction abort.
   */
  [[nodiscard]] bool is_transaction_abort() const noexcept { return _is_transaction_abort; }

  /**
   * @return The number of cycles of the branch (zero if not supported on the underlying hardware).
   */
  [[nodiscard]] std::optional<std::uint16_t> cycles() const noexcept { return _cycles; }

  /**
   * @return The hardware classification of the branch instruction, or std::nullopt if PERF_SAMPLE_BRANCH_TYPE_SAVE
   *         was not requested or the kernel is older than 4.15.
   */
  [[nodiscard]] std::optional<Classification> classification() const noexcept { return _classification; }

  /**
   * @return The speculation outcome of the branch, or std::nullopt if the kernel is older than 6.1.
   */
  [[nodiscard]] std::optional<Speculation> speculation_result() const noexcept { return _speculation_result; }

private:
  std::uintptr_t _instruction_pointer_from;
  std::uintptr_t _instruction_pointer_to;
  bool _is_mispredicted;
  bool _is_predicted;
  bool _is_in_transaction;
  bool _is_transaction_abort;
  std::optional<std::uint16_t> _cycles;

  /// Branch classification from the hardware; nullopt if PERF_SAMPLE_BRANCH_TYPE_SAVE was not requested or kernel
  /// < 4.15.
  std::optional<Classification> _classification;

  /// Branch speculation outcome from the hardware; nullopt if kernel < 6.1.
  std::optional<Speculation> _speculation_result;
};
}