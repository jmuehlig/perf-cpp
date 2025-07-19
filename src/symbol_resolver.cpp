#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <perfcpp/exception.h>
#include <perfcpp/symbol_resolver.h>
#include <regex>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <perfcpp/util/unique_file_descriptor.h>

perf::SymbolResolver::SymbolResolver()
{
  for (auto& module : SymbolResolver::parse_maps()) {
    if (auto symbols = SymbolResolver::parse_symbol_table(module); !symbols.empty()) {
      this->_modules.insert(std::make_pair(std::move(module), std::move(symbols)));
    }
  }
}

std::optional<perf::SymbolResolver::ResolvedSymbol>
perf::SymbolResolver::resolve(const std::uintptr_t logical_instruction_pointer) const noexcept
{
  for (const auto& [module, symbols] : this->_modules) {
    if (logical_instruction_pointer >= module.start() && logical_instruction_pointer < module.end()) {
      return SymbolResolver::resolve(module, symbols, logical_instruction_pointer);
    }
  }

  return std::nullopt;
}

std::optional<perf::SymbolResolver::ResolvedSymbol>
perf::SymbolResolver::resolve(const perf::SymbolResolver::Module& module,
                              const std::vector<Symbol>& symbols,
                              std::uintptr_t logical_instruction_pointer) noexcept
{
  const auto relative_address = logical_instruction_pointer - module.start() + module.offset();

  /// Find the closest symbol.
  auto closest_symbol_iterator =
    std::upper_bound(symbols.begin(), symbols.end(), relative_address, [](const auto address, const auto& symbol) {
      return address < symbol.address();
    });

  if (closest_symbol_iterator == symbols.begin()) {
    return std::nullopt;
  }

  /// Get the symbol at or before our address.
  --closest_symbol_iterator;

  // Check if we're within the symbol's range
  if (!closest_symbol_iterator->is_in_range(relative_address)) {
    return std::nullopt;
  }

  return ResolvedSymbol{ module, *closest_symbol_iterator, relative_address - closest_symbol_iterator->address() };
}

std::vector<perf::SymbolResolver::Module>
perf::SymbolResolver::parse_maps()
{
  auto maps_stream = std::ifstream{ "/proc/self/maps" };
  if (!maps_stream.is_open()) {
    return {};
  }

  auto maps = std::vector<Module>{};
  maps.reserve(32U);

  std::string line;
  const auto map_regex =
    std::regex{ R"(([0-9a-f]+)-([0-9a-f]+)\s+([rwxp-]+)\s+([0-9a-f]+)\s+[0-9a-f]+:[0-9a-f]+\s+\d+\s*(.*)?)" };

  while (std::getline(maps_stream, line)) {
    std::smatch match;
    if (std::regex_match(line, match, map_regex)) {
      auto path = match[5].str();
      auto permission = match[3].str();

      if (permission.find('x') != std::string::npos && !path.empty() && path.front() == '/') {
        /// Turn path into module name.
        const auto pos = path.find_last_of('/');
        auto module_name = (pos != std::string::npos) ? path.substr(pos + 1U) : path;

        maps.emplace_back(std::move(module_name),
                          std::stoull(match[1].str(), nullptr, 16),
                          std::stoull(match[2].str(), nullptr, 16),
                          std::stoull(match[4].str(), nullptr, 16),
                          std::move(path),
                          std::move(permission));
      }
    }
  }

  return maps;
}

std::vector<perf::SymbolResolver::Symbol>
perf::SymbolResolver::parse_symbol_table(const perf::SymbolResolver::Module& module)
{
  const auto file_descriptor = util::UniqueFileDescriptor{::open(module.path().c_str(), O_RDONLY)};
  if (!file_descriptor.has_value()) {
    throw CannotReadSymbolsForModule{ module.name(), module.path() };
  }

  struct stat stat_ {};
  if (::fstat(file_descriptor.value(), &stat_) < 0) {
    throw CannotReadFstatForModule{ module.name(), module.path() };
  }

  const auto stat_size = std::size_t(stat_.st_size);

  /// MMap ELF data.
  auto* elf_data = ::mmap(nullptr, stat_size, PROT_READ, MAP_PRIVATE, file_descriptor.value(), 0);
  if (elf_data == MAP_FAILED) {
    throw CannotReadElfForModule{ module.name(), module.path() };
  }
  auto* elf_header = static_cast<const Elf64_Ehdr*>(elf_data);

  /// Verify ELF magic.
  if (std::memcmp(elf_header->e_ident, ELFMAG, SELFMAG) != 0) {
    ::munmap(elf_data, stat_size);
    throw CannotVerifyElfMagicForModule{ module.name(), module.path() };
  }

  /// Find the symbol and string tables.
  const auto* section_header_table = reinterpret_cast<const Elf64_Shdr*>(static_cast<const char*>(elf_data) + elf_header->e_shoff);
  const auto [symbol_table, string_table] = SymbolResolver::find_symbol_and_string_tables(section_header_table, elf_header->e_shnum);

  if (!symbol_table || !string_table) {
    ::munmap(elf_data, stat_size);
    return {};
  }

  /// Access symbols.
  const auto* symbols = reinterpret_cast<const Elf64_Sym*>(static_cast<char*>(elf_data) + symbol_table->sh_offset);
  const auto* strings = static_cast<char*>(elf_data) + string_table->sh_offset;
  const auto symbols_size = symbol_table->sh_size / sizeof(Elf64_Sym);

  auto extracted_symbols = std::vector<Symbol>{};
  extracted_symbols.reserve(symbols_size);

  /// Read symbols and transform to Symbol instances.
  for (auto i = 0ULL; i < symbols_size; ++i) {
    if (ELF64_ST_TYPE(symbols[i].st_info) == STT_FUNC && symbols[i].st_name) {
      if (auto name = std::string(strings + symbols[i].st_name); !name.empty()) {
        extracted_symbols.emplace_back(std::move(name), symbols[i].st_value, symbols[i].st_size);
      }
    }
  }

  /// Unmap ELF data.
  ::munmap(elf_data, stat_size);

  /// Sort the symbols to enable an upper bound search later.
  std::sort(extracted_symbols.begin(), extracted_symbols.end(), [](const Symbol& first, const Symbol& second) {
    return first.address() < second.address();
  });

  return extracted_symbols;
}

std::pair<const Elf64_Shdr*, const Elf64_Shdr*>
perf::SymbolResolver::find_symbol_and_string_tables(const Elf64_Shdr* section_header_table, const std::uint16_t size) noexcept
{
  for (auto i = 0U; i < size; ++i) {
    if (section_header_table[i].sh_type == SHT_SYMTAB) {
      return std::make_pair(
        &section_header_table[i], &section_header_table[section_header_table[i].sh_link]);
    }
  }

  return std::make_pair(nullptr, nullptr);
}