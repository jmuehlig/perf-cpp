#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace perf {
class Table
{
public:
  class Column
  {
  public:
    explicit Column(std::string&& content) noexcept
      : _content(std::move(content))
    {
    }
    explicit Column(const std::string& content)
      : _content(content)
    {
    }
    Column(std::string&& content, const std::uint8_t span) noexcept
      : _span(span)
      , _content(std::move(content))
    {
    }
    Column(const std::string& content, const std::uint8_t span)
      : _span(span)
      , _content(content)
    {
    }

    [[nodiscard]] std::uint8_t span() const noexcept { return _span; }
    [[nodiscard]] const std::string& content() const noexcept { return _content; }

  private:
    std::uint8_t _span{ 1U };
    std::string _content;
  };

  class Row
  {
  public:
    Row() = default;
    explicit Row(const std::size_t reserve_columns) { _columns.reserve(reserve_columns); }
    ~Row() = default;

    void add(Column&& column) { _columns.push_back(std::move(column)); }
    Row& operator<<(Column&& column)
    {
      _columns.push_back(std::move(column));
      return *this;
    }

    Row& operator<<(std::string&& column)
    {
      _columns.emplace_back(std::move(column));
      return *this;
    }

    Row& operator<<(const std::string& column)
    {
      _columns.emplace_back(std::string{ column });
      return *this;
    }

    Row& operator<<(const std::size_t column)
    {
      _columns.emplace_back(std::to_string(column));
      return *this;
    }

    Row& operator<<(const std::uint32_t column)
    {
      _columns.emplace_back(std::to_string(column));
      return *this;
    }

    Row& operator<<(const std::uint16_t column)
    {
      _columns.emplace_back(std::to_string(column));
      return *this;
    }

    Row& operator<<(const std::uint8_t column)
    {
      _columns.emplace_back(std::to_string(column));
      return *this;
    }

    Row& operator<<(const float column)
    {
      _columns.emplace_back(std::to_string(column));
      return *this;
    }

    Row& operator<<(const double column)
    {
      _columns.emplace_back(std::to_string(column));
      return *this;
    }

    [[nodiscard]] const std::vector<Column>& columns() const noexcept { return _columns; }
    [[nodiscard]] std::vector<Column>& columns() noexcept { return _columns; }

  private:
    std::vector<Column> _columns;
  };

  enum class Alignment : std::uint8_t
  {
    Left,
    Center,
    Right
  };

  Table() = default;
  explicit Table(std::vector<Alignment>&& column_alignments)
    : _alignments(std::move(column_alignments))
  {
  }
  explicit Table(const std::uint64_t offset)
    : _offset(offset)
  {
  }
  Table(const std::uint64_t offset, std::vector<Alignment>&& column_alignments)
    : _alignments(std::move(column_alignments))
    , _offset(offset)
  {
  }
  ~Table() = default;

  /**
   * Reserve space for the given number of rows.
   * @param count_rows Number of rows to reserve.
   */
  void reserve(const std::size_t count_rows) { _rows.reserve(count_rows); }

  /**
   * Set separators for columns.
   * @param separators List of separators, one for each column.
   */
  void column_separators(std::vector<char>&& separators) { _column_separators = std::move(separators); }

  /**
   * Adds the given row to the table.
   * @param row Row to add.
   */
  void add(Row&& row);

  /**
   * @return Turns the table into a printable string.
   */
  [[nodiscard]] std::string to_string() const;

private:
  /// Alignment for each column.
  std::vector<Alignment> _alignments;

  /// Separators for columns.
  std::vector<char> _column_separators;

  /// Rows.
  std::vector<Row> _rows;

  /// Offset of each row in number of empty spaces.
  std::uint64_t _offset{ 0U };
};
}