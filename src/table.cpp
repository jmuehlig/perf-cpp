#include <iomanip>
#include <numeric>
#include <perfcpp/table.h>
#include <sstream>

void
perf::Table::add(perf::Table::Row&& row)
{
  /// Add alignments, if the row is larger than the current known columns.
  if (row.columns().size() > this->_alignments.size()) {
    for (auto i = 0U; i < (row.columns().size() - this->_alignments.size()); ++i) {
      this->_alignments.emplace_back(Alignment::Left);
    }
  }

  /// Add separator, if the row is larger than the current known columns.
  if (row.columns().size() > this->_column_separators.size()) {
    for (auto i = 0U; i < (row.columns().size() - this->_column_separators.size()); ++i) {
      this->_column_separators.emplace_back('\0');
    }
  }

  /// Add the row.
  this->_rows.push_back(std::move(row));
}

std::string
perf::Table::to_string() const
{
  /// Calculate the max size for each column.
  auto column_max_sizes = std::vector<std::size_t>{};
  column_max_sizes.resize(this->_alignments.size(), 0U);

  for (const auto& row : this->_rows) {
    auto column_id = 0UL;
    for (const auto& column : row.columns()) {
      if (column.span() == 1U) {
        column_max_sizes[column_id] = std::max(column_max_sizes[column_id], column.content().size());
      }
      column_id += column.span();
    }
  }

  /// Print the rows.
  auto stream = std::stringstream{};

  for (const auto& row : this->_rows) {

    /// Add offset at the beginning of the row.
    if (this->_offset > 0U) {
      stream << std::string(this->_offset, ' ');
    }

    /// Print all columns.
    auto column_id = 0ULL;
    for (const auto& column : row.columns()) {
      /// Add a space left.
      if (column_id > 0ULL) {
        stream << " ";
      }

      /// Add left column separator.
      const auto has_left_separator = this->_column_separators[column_id] != '\0';
      if (has_left_separator) {
        stream << this->_column_separators[column_id];
      }

      stream << " ";

      /// Content of "normal" columns that do not span over multiple columns.
      if (column.span() == 1U) {
        switch (this->_alignments[column_id]) {
          case Alignment::Left:
            /// Add content and empty spaces to the right..
            stream << column.content() << std::string(column_max_sizes[column_id] - column.content().size(), ' ');
            break;
          case Alignment::Right:
            /// Add empty spaces to the left and content.
            stream << std::string(column_max_sizes[column_id] - column.content().size(), ' ') << column.content();
            break;
          case Alignment::Center:
            /// Calculate the number of empty spaces to the left and to the right.
            const auto total_spaces = column_max_sizes[column_id] - column.content().size();
            const auto left_spaces = total_spaces / 2U;
            const auto right_spaces = total_spaces - left_spaces;

            /// Add empty spaces to the left, content, and empty spaces to the right.
            stream << std::string(left_spaces, ' ') << column.content() << std::string(right_spaces, ' ');
            break;
        }
      }

      /// Content of columns that span over multiple columns.
      else {
        /// Aggregate the size of this cell as the size of all spanned columns plus separators.
        auto span_columns_sizes = column_max_sizes[column_id];
        for (auto i = column_id + 1U; i < column_id + column.span(); ++i) {
          span_columns_sizes += /* size of the spanned column */ column_max_sizes[i] +
                                /* empty space for each column */ 2U +
                                /* separator, if present */ (this->_column_separators[i] != '\0' * 1U);
        }

        /// Calculate empty spaces to the left and to the right.
        const auto total_spaces = span_columns_sizes - column.content().size();
        const auto left_spaces = total_spaces / 2U;
        const auto right_spaces = total_spaces - left_spaces;

        stream << std::string(left_spaces, ' ') << column.content() << std::string(right_spaces, ' ');
      }

      column_id += column.span();
    }

    stream << "\n";
  }

  return stream.str();
}