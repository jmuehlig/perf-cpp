#pragma once
#include <cstdint>
#include <elf.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace perf {
class SymbolResolver
{
public:
  class Module
  {
  public:
    Module(std::string&& name,
           const std::uintptr_t start,
           const std::uintptr_t end,
           const std::size_t offset,
           std::string&& path,
           std::string&& permission) noexcept
      : _name(std::move(name))
      , _start(start)
      , _end(end)
      , _offset(offset)
      , _path(std::move(path))
      , _permissions(std::move(permission))
    {
    }

    Module(std::string&& name,
           const std::uintptr_t start,
           const std::uintptr_t end,
           const std::size_t offset,
           std::string&& path,
           std::string&& permission,
           std::vector<std::uint8_t>&& build_id) noexcept
      : _name(std::move(name))
      , _start(start)
      , _end(end)
      , _offset(offset)
      , _path(std::move(path))
      , _permissions(std::move(permission))
      , _build_id(std::move(build_id))
    {
    }

    ~Module() = default;

    [[nodiscard]] const std::string& name() const noexcept { return _name; }
    [[nodiscard]] std::uintptr_t start() const noexcept { return _start; }
    [[nodiscard]] std::uintptr_t end() const noexcept { return _end; }
    [[nodiscard]] std::size_t offset() const noexcept { return _offset; }
    [[nodiscard]] const std::string& path() const noexcept { return _path; }
    [[nodiscard]] const std::string& permission() const noexcept { return _permissions; }
    [[nodiscard]] const std::vector<std::uint8_t>& build_id() const noexcept { return _build_id; }

    [[nodiscard]] bool operator==(const Module& other) const { return _path == other._path; }

  private:
    std::string _name;
    std::uintptr_t _start;
    std::uintptr_t _end;
    std::uintptr_t _offset;
    std::string _path;
    std::string _permissions;
    std::vector<std::uint8_t> _build_id;
  };

  class ModuleHash
  {
  public:
    std::size_t operator()(const Module& module) const { return std::hash<std::string>()(module.path()); }
  };

  class Symbol
  {
  public:
    Symbol(std::string&& name, const std::uintptr_t address, const std::size_t size) noexcept
      : _name(std::move(name))
      , _address(address)
      , _size(size)
    {
    }
    ~Symbol() = default;

    [[nodiscard]] const std::string& name() const noexcept { return _name; }
    [[nodiscard]] std::uintptr_t address() const noexcept { return _address; }
    [[nodiscard]] std::size_t size() const noexcept { return _size; }

    [[nodiscard]] bool is_in_range(const std::uintptr_t address) const noexcept
    {
      return address >= _address && address < (_address + _size);
    }

    [[nodiscard]] bool operator==(const Symbol& other) const noexcept { return _address == other._address; }

    [[nodiscard]] std::string to_string() const
    {
      auto name = _name;
      return name.append(" (")
        .append(std::to_string(_address))
        .append("--")
        .append(std::to_string(_address + size()))
        .append(")");
    }

  private:
    std::string _name;
    std::uintptr_t _address;
    std::size_t _size;
  };

  class ResolvedSymbol
  {
  public:
    ResolvedSymbol(const Module& module, const Symbol& symbol, const std::size_t offset) noexcept
      : _module(module)
      , _symbol(symbol)
      , _offset(offset)
    {
    }
    ~ResolvedSymbol() noexcept = default;

    [[nodiscard]] const Module& module() const noexcept { return _module; }
    [[nodiscard]] const Symbol& symbol() const noexcept { return _symbol; }
    [[nodiscard]] std::size_t offset() const noexcept { return _offset; }

    [[nodiscard]] std::string to_string() const
    {
      auto str = std::string{ "[" };
      return str.append(_module.name()).append("] ").append(_symbol.name()).append("+").append(std::to_string(_offset));
    }

  private:
    const Module& _module;
    const Symbol& _symbol;
    const std::size_t _offset;
  };

  SymbolResolver();
  ~SymbolResolver() = default;

  /**
   * Resolves the symbol the given instruction points to.
   *
   * @param logical_instruction_pointer Logical instruction pointer.
   * @return The resolved symbol, if the symbol can be resolved.
   */
  [[nodiscard]] std::optional<ResolvedSymbol> resolve(std::uintptr_t logical_instruction_pointer) const noexcept;

  /**
   * Parses the /proc/self/maps table.
   *
   * @return List of all modules found in /proc/self/maps.
   */
  [[nodiscard]] static std::vector<Module> read_modules();

  /**
   * Reads the process name from /proc/self/comm.
   * @return Name of the process, if it can be read.
   */
  [[nodiscard]] static std::optional<std::string> read_process_name();

private:
  /// List of all modules and linked symbols.
  std::unordered_map<Module, std::vector<Symbol>, ModuleHash> _modules;

  /**
   * Resolves the symbol within the given module (and symbols).
   *
   * @param module Module the instruction points to.
   * @param symbols Symbols of the module.
   * @param logical_instruction_pointer Instruction pointer to resolve.
   * @return Symbol if it can be resolved.
   */
  [[nodiscard]] static std::optional<ResolvedSymbol> resolve(const Module& module,
                                                             const std::vector<Symbol>& symbols,
                                                             std::uintptr_t logical_instruction_pointer) noexcept;

  /**
   * Parses the symbol table for the given module (path).
   *
   * @param module Module to lookup.
   * @return List of all symbols linked to the module.
   */
  [[nodiscard]] static std::vector<Symbol> parse_symbol_table(const perf::SymbolResolver::Module& module);

  /**
   * Scans the section header table for the symbol table and string table.
   *
   * @param section_header_table Pointer to the section header table.
   * @param size Size of the section header table.
   * @return Pointer pair (symbol table, string table).
   */
  [[nodiscard]] static std::pair<const Elf64_Shdr*, const Elf64_Shdr*> find_symbol_and_string_tables(
    const Elf64_Shdr* section_header_table,
    std::uint16_t size) noexcept;

  /**
   * Extracts the build ID from an ELF file.
   *
   * @param path Path of the module.
   * @return Build ID as a vector of bytes, or empty if not found.
   */
  [[nodiscard]] static std::vector<std::uint8_t> extract_build_id(const std::string& path) noexcept;
};
}