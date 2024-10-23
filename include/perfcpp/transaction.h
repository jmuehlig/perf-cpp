#pragma once

#include <cstdint>
#include <linux/perf_event.h>

namespace perf
{
class TransactionAbort
{
public:
  TransactionAbort(const std::uint64_t transaction_abort_mask) noexcept : _transaction_abort_mask(transaction_abort_mask) {}
  ~TransactionAbort() noexcept = default;

  /**
   * @return True, if the abort comes from an elision type transaction (Intel-specific).
   */
  [[nodiscard]] bool is_elision() const noexcept { return _transaction_abort_mask & PERF_TXN_ELISION;  }

  /**
   * @return True, if the abort comes a generic transaction.
   */
  [[nodiscard]] bool is_transaction() const noexcept { return _transaction_abort_mask & PERF_TXN_TRANSACTION;  }

  /**
   * @return True, if the abort is synchronous. The instruction pointer will point to the related instruction.
   */
  [[nodiscard]] bool is_synchronous() const noexcept { return _transaction_abort_mask & PERF_TXN_SYNC;  }

  /**
   * @return True, if the abort is asynchronous.
   */
  [[nodiscard]] bool is_asynchronous() const noexcept { return _transaction_abort_mask & PERF_TXN_ASYNC;  }

  /**
   * @return True, if the abort is retryable.
   */
  [[nodiscard]] bool is_retry() const noexcept { return _transaction_abort_mask & PERF_TXN_RETRY;  }

  /**
   * @return True, if the abort is due to a conflict.
   */
  [[nodiscard]] bool is_conflict() const noexcept { return _transaction_abort_mask & PERF_TXN_CONFLICT;  }

  /**
   * @return True, if the abort is due to write capacity.
   */
  [[nodiscard]] bool is_capacity_write() const noexcept { return _transaction_abort_mask & PERF_TXN_CAPACITY_WRITE;  }

  /**
   * @return True, if the abort is due to read capacity.
   */
  [[nodiscard]] bool is_capacity_read() const noexcept { return _transaction_abort_mask & PERF_TXN_CAPACITY_READ;  }

  /**
   * @return Returns the user-specified code for the transaction abort.
   */
  [[nodiscard]] std::uint32_t code() const noexcept { return (_transaction_abort_mask >> PERF_TXN_ABORT_SHIFT) & PERF_TXN_ABORT_MASK;  }
private:
  std::uint64_t _transaction_abort_mask;
};
}