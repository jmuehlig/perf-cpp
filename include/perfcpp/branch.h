#pragma once

#include <cstdint>
#include <linux/perf_event.h>

namespace perf {
enum BranchType : std::uint64_t
{
  None = 0ULL,

  User = PERF_SAMPLE_BRANCH_USER,
  Kernel = PERF_SAMPLE_BRANCH_KERNEL,
  HyperVisor = PERF_SAMPLE_BRANCH_HV,

  Any = PERF_SAMPLE_BRANCH_ANY,
  Call = PERF_SAMPLE_BRANCH_ANY_CALL,
  DirectCall = PERF_SAMPLE_BRANCH_CALL,
  IndirectCall = PERF_SAMPLE_BRANCH_IND_CALL,
  Return = PERF_SAMPLE_BRANCH_ANY_RETURN,
  IndirectJump = PERF_SAMPLE_BRANCH_IND_JUMP,
  Conditional = PERF_SAMPLE_BRANCH_COND,
  TransactionalMemoryAbort = PERF_SAMPLE_BRANCH_ABORT_TX,
  InTransaction = PERF_SAMPLE_BRANCH_IN_TX,
  NotInTransaction = PERF_SAMPLE_BRANCH_NO_TX
};
}