#include <algorithm>
#include <cstring>
#include <elf.h>
#include <fcntl.h>
#include <fstream>
#include <perfcpp/exception.h>
#include <perfcpp/symbol_resolver.h>
#include <regex>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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
  const auto file_descriptor = ::open(module.path().c_str(), O_RDONLY);
  if (file_descriptor < 0) {
    throw CannotReadSymbolsForModule{ module.name(), module.path() };
  }

  struct stat stat_
  {};
  if (::fstat(file_descriptor, &stat_) < 0) {
    ::close(file_descriptor);
    throw CannotReadFstatForModule{ module.name(), module.path() };
  }

  const auto stat_size = std::size_t(stat_.st_size);

  /// Read ELF data.
  auto* elf_data = ::mmap(nullptr, stat_size, PROT_READ, MAP_PRIVATE, file_descriptor, 0);
  if (elf_data == MAP_FAILED) {
    ::close(file_descriptor);
    throw CannotReadElfForModule{ module.name(), module.path() };
  }

  auto* ehdr = static_cast<Elf64_Ehdr*>(elf_data);

  /// Verify ELF magic.
  if (std::memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
    ::munmap(elf_data, stat_size);
    ::close(file_descriptor);
    throw CannotVerifyElfMagicForModule{ module.name(), module.path() };
  }

  auto* shdr = reinterpret_cast<Elf64_Shdr*>(static_cast<char*>(elf_data) + ehdr->e_shoff);
  Elf64_Shdr* symtab = nullptr;
  Elf64_Shdr* strtab = nullptr;

  /// Find symbol table and string table.
  for (auto i = 0U; i < ehdr->e_shnum; ++i) {
    if (shdr[i].sh_type == SHT_SYMTAB) {
      symtab = &shdr[i];
      strtab = &shdr[shdr[i].sh_link];
      break;
    }
  }

  if (!symtab || !strtab) {
    ::munmap(elf_data, stat_size);
    ::close(file_descriptor);
    return {};
  }

  auto* syms = reinterpret_cast<Elf64_Sym*>(static_cast<char*>(elf_data) + symtab->sh_offset);
  auto* strings = static_cast<char*>(elf_data) + strtab->sh_offset;
  const auto sym_count = symtab->sh_size / sizeof(Elf64_Sym);

  auto symbols = std::vector<Symbol>{};
  symbols.reserve(sym_count);

  /// Read symbols.
  for (auto i = 0ULL; i < sym_count; ++i) {
    if (ELF64_ST_TYPE(syms[i].st_info) == STT_FUNC && syms[i].st_name) {
      if (auto name = std::string(strings + syms[i].st_name); !name.empty()) {
        symbols.emplace_back(std::move(name), syms[i].st_value, syms[i].st_size);
      }
    }
  }

  ::munmap(elf_data, stat_size);
  ::close(file_descriptor);

  /// Sort the symbols to enable an upper bound search later.
  std::sort(symbols.begin(), symbols.end(), [](const Symbol& first, const Symbol& second) {
    return first.address() < second.address();
  });

  return symbols;
}