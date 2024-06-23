#include <algorithm>
#include <asm/unistd.h>
#include <cstring>
#include <exception>
#include <iostream>
#include <numeric>
#include <perfcpp/sampler.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

perf::Sampler::Sampler(const perf::CounterDefinition& counter_list,
                       std::vector<std::string>&& counter_names,
                       const std::uint64_t type,
                       perf::SampleConfig config)
  : _counter_definitions(counter_list)
  , _config(config)
  , _sample_type(type)
{
  /// Check if any unsupported types are used.
  if (type & (std::uint64_t(1U) << 63U)) {
    throw std::runtime_error{ "The used sample types are not (all) supported by the Linux Kernel version (e.g., "
                              "DataPageSize and CodePageSize are implemented since 5.11)." };
  }

  for (const auto& counter_name : counter_names) {
    if (!this->_counter_definitions.is_metric(counter_name)) /// Metrics are not (yet) supported.
    {
      /// Try to set the counter, if the name refers to a counter.
      if (auto counter_config = this->_counter_definitions.counter(counter_name); counter_config.has_value()) {
        this->_group.add(counter_config.value());
        this->_counter_names.push_back(counter_name);
      }
    }
  }
}

bool
perf::Sampler::open()
{
  if (this->_group.empty()) {
    return false;
  }

  /// Detect, if the leader is an auxiliary (specifically for Sapphire Rapids).
  const auto is_leader_auxiliary_counter = this->_group.member(0U).is_auxiliary();

  for (auto counter_index = 0U; counter_index < this->_group.size(); ++counter_index) {
    auto& counter = this->_group.member(counter_index);

    /// The first counter in the group has a "special" role, others will use its file descriptor.
    const auto is_leader = counter_index == 0U;

    /// For Intel's Sapphire Rapids architecture, sampling for memory requires a dummy as first counter.
    /// Only the second counter is the "real" sampling counter.
    const auto is_secret_leader = is_leader_auxiliary_counter && counter_index == 1U;

    auto& perf_event = counter.event_attribute();
    std::memset(&perf_event, 0, sizeof(perf_event_attr));
    perf_event.type = counter.type();
    perf_event.size = sizeof(perf_event_attr);
    perf_event.config = counter.event_id();
    perf_event.config1 = counter.event_id_extension()[0U];
    perf_event.config2 = counter.event_id_extension()[1U];
    perf_event.disabled = is_leader;

    perf_event.inherit = static_cast<std::int32_t>(this->_config.is_include_child_threads());
    perf_event.exclude_kernel = static_cast<std::int32_t>(!this->_config.is_include_kernel());
    perf_event.exclude_user = static_cast<std::int32_t>(!this->_config.is_include_user());
    perf_event.exclude_hv = static_cast<std::int32_t>(!this->_config.is_include_hypervisor());
    perf_event.exclude_idle = static_cast<std::int32_t>(!this->_config.is_include_idle());
    perf_event.exclude_guest = static_cast<std::int32_t>(!this->_config.is_include_guest());

    if (is_leader || is_secret_leader) {
      perf_event.sample_type = this->_sample_type;

      if (this->_config.is_frequency()) {
        perf_event.freq = 1U;
        perf_event.sample_freq = this->_config.frequency_or_period();
      } else {
        perf_event.sample_period = this->_config.frequency_or_period();
      }

      if (this->_sample_type & Sampler::Type::BranchStack) {
        perf_event.branch_sample_type = this->_config.branch_type();
      }

      if (is_leader) {
        perf_event.mmap = 1U;
      }

      if (this->_sample_type & static_cast<std::uint64_t>(Type::Callchain)) {
        perf_event.sample_max_stack = this->_config.max_stack();
      }
    }

    if (this->_sample_type & static_cast<std::uint64_t>(Type::CounterValues)) {
      perf_event.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    }

    if (this->_sample_type & static_cast<std::uint64_t>(Type::UserRegisters)) {
      perf_event.sample_regs_user = this->_config.user_registers().mask();
    }

    if (this->_sample_type & static_cast<std::uint64_t>(Type::KernelRegisters)) {
      perf_event.sample_regs_intr = this->_config.kernel_registers().mask();
    }

    /// Open the counter. Try to decrease the precise_ip if the file syscall was not successful, reporting an invalid
    /// argument.
    std::int32_t file_descriptor;
    for (auto precise_ip = std::int32_t{ this->_config.precise_ip() }; precise_ip > -1; --precise_ip) {
      perf_event.precise_ip = std::uint64_t(precise_ip);
      file_descriptor = syscall(__NR_perf_event_open, &perf_event, 0, -1, this->_group.leader_file_descriptor(), 0);
      if (file_descriptor > -1 || errno != EINVAL) {
        break;
      }
    }
    counter.file_descriptor(file_descriptor);

    /// Print debug output, if requested.
    if (this->_config.is_debug()) {
      std::cout << counter.to_string() << std::flush;
    }

    /// Check if the counter could be opened successfully.
    if (file_descriptor < 0) {
      this->_last_error = errno;
      return false;
    }
  }

  /// Open the mapped buffer.
  /// If the leader is an "auxiliary" counter (like on Sapphire Rapid), use the second counter instead.
  const auto file_descriptor = is_leader_auxiliary_counter && this->_group.size() > 1U
                                 ? this->_group.member(1U).file_descriptor()
                                 : this->_group.leader_file_descriptor();

  this->_buffer =
    ::mmap(nullptr, this->_config.buffer_pages() * 4096U, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
  if (this->_buffer == MAP_FAILED) {
    this->_last_error = errno;
    return false;
  }

  return this->_buffer != nullptr;
}

bool
perf::Sampler::start()
{
  /// Start the counters.
  if (this->open()) {
    ::ioctl(this->_group.leader_file_descriptor(), PERF_EVENT_IOC_RESET, 0);
    ::ioctl(this->_group.leader_file_descriptor(), PERF_EVENT_IOC_ENABLE, 0);
    return true;
  }

  return false;
}

void
perf::Sampler::stop()
{
  ::ioctl(this->_group.leader_file_descriptor(), PERF_EVENT_IOC_DISABLE, 0);
}

void
perf::Sampler::close()
{
  ::munmap(this->_buffer, this->_config.buffer_pages() * 4096U);
  ::close(this->_group.leader_file_descriptor());
}

std::vector<perf::Sample>
perf::Sampler::result() const
{
  auto result = std::vector<Sample>{};
  if (this->_buffer == nullptr) {
    return result;
  }

  auto* mmap_page = reinterpret_cast<perf_event_mmap_page*>(this->_buffer);

  /// When the ringbuffer is empty or already read, there is nothing to do.
  if (mmap_page->data_tail >= mmap_page->data_head) {
    return result;
  }

  result.reserve(2048U);

  /// The buffer starts at page 1 (from 0).
  auto iterator = std::uintptr_t(this->_buffer) + 4096U;

  /// data_head is the size (in bytes) of the samples.
  const auto end = iterator + mmap_page->data_head;

  while (iterator < end) {
    auto* event_header = reinterpret_cast<perf_event_header*>(iterator);

    if (event_header->type == PERF_RECORD_SAMPLE) {
      auto mode = Sample::Mode::Unknown;
      if (static_cast<bool>(event_header->misc & PERF_RECORD_MISC_KERNEL)) {
        mode = Sample::Mode::Kernel;
      } else if (static_cast<bool>(event_header->misc & PERF_RECORD_MISC_USER)) {
        mode = Sample::Mode::User;
      } else if (static_cast<bool>(event_header->misc & PERF_RECORD_MISC_HYPERVISOR)) {
        mode = Sample::Mode::Hypervisor;
      } else if (static_cast<bool>(event_header->misc & PERF_RECORD_MISC_GUEST_KERNEL)) {
        mode = Sample::Mode::GuestKernel;
      } else if (static_cast<bool>(event_header->misc & PERF_RECORD_MISC_GUEST_USER)) {
        mode = Sample::Mode::GuestUser;
      }

      auto sample = Sample{ mode };

      auto sample_ptr = std::uintptr_t(reinterpret_cast<void*>(event_header + 1U));

      if (this->_sample_type & perf::Sampler::Type::Identifier) {
        sample.sample_id(*reinterpret_cast<std::uint64_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint64_t);
      }

      if (this->_sample_type & perf::Sampler::Type::InstructionPointer) {
        sample.instruction_pointer(*reinterpret_cast<std::uintptr_t*>(sample_ptr));
        sample_ptr += sizeof(std::uintptr_t);
      }

      if (this->_sample_type & perf::Sampler::Type::ThreadId) {
        sample.process_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint32_t);

        sample.thread_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint32_t);
      }

      if (this->_sample_type & perf::Sampler::Type::Time) {
        sample.timestamp(*reinterpret_cast<std::uint64_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint64_t);
      }

      if (this->_sample_type & perf::Sampler::Type::LogicalMemAddress) {
        sample.logical_memory_address(*reinterpret_cast<std::uint64_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint64_t);
      }

      if (this->_sample_type & perf::Sampler::Type::CPU) {
        sample.cpu_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint64_t);
      }

      if (this->_sample_type & perf::Sampler::Type::Period) {
        sample.period(*reinterpret_cast<std::uint64_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint64_t);
      }

      if (this->_sample_type & perf::Sampler::Type::CounterValues) {
        auto* read_format = reinterpret_cast<Sampler::read_format*>(sample_ptr);
        const auto count_counter_values = read_format->count_members;

        if (count_counter_values == this->_group.size()) {
          auto counter_values = std::vector<std::pair<std::string, double>>{};
          for (auto counter_id = 0U; counter_id < this->_group.size(); ++counter_id) {
            counter_values.emplace_back(this->_counter_names[counter_id],
                                        double(read_format->values[counter_id].value));
          }

          sample.counter_result(CounterResult{ std::move(counter_values) });
        }

        sample_ptr +=
          sizeof(Sampler::read_format::count_members) + count_counter_values * sizeof(Sampler::read_format::value);
      }

      if (this->_sample_type & perf::Sampler::Type::Callchain) {
        const auto callchain_size = (*reinterpret_cast<std::uint64_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint64_t);

        if (callchain_size > 0U) {
          auto callchain = std::vector<std::uintptr_t>{};
          callchain.reserve(callchain_size);

          auto* instruction_pointers = reinterpret_cast<std::uint64_t*>(sample_ptr);
          for (auto index = 0U; index < callchain_size; ++index) {
            callchain.push_back(std::uintptr_t{ instruction_pointers[index] });
          }

          sample.callchain(std::move(callchain));

          sample_ptr += callchain_size * sizeof(std::uint64_t);
        }
      }

      if (this->_sample_type & perf::Sampler::BranchStack) {
        const auto count_branches = *reinterpret_cast<std::uint64_t*>(sample_ptr);
        sample_ptr += sizeof(std::uint64_t);

        if (count_branches > 0U) {
          auto branches = std::vector<Branch>{};
          branches.reserve(count_branches);

          auto* sampled_branches = reinterpret_cast<perf_branch_entry*>(sample_ptr);
          for (auto i = 0U; i < count_branches; ++i) {
            const auto& branch = sampled_branches[i];
            branches.emplace_back(
              branch.from, branch.to, branch.mispred, branch.predicted, branch.in_tx, branch.abort, branch.cycles);
          }

          sample.branches(std::move(branches));
        }

        sample_ptr += sizeof(perf_branch_entry) * count_branches;
      }

      if (this->_sample_type & perf::Sampler::UserRegisters) {
        const auto abi = *reinterpret_cast<std::uint64_t*>(sample_ptr);
        sample.user_registers_abi(abi);
        sample_ptr += sizeof(std::uint64_t);

        const auto count_user_registers = this->_config.user_registers().size();
        if (count_user_registers > 0U) {
          auto user_registers = std::vector<std::uint64_t>{};
          user_registers.reserve(count_user_registers);

          const auto* perf_user_registers = reinterpret_cast<std::uint64_t*>(sample_ptr);
          for (auto register_id = 0U; register_id < count_user_registers; ++register_id) {
            user_registers.push_back(perf_user_registers[register_id]);
          }

          sample.user_registers(std::move(user_registers));

          sample_ptr += sizeof(std::uint64_t) * count_user_registers;
        }
      }

      if (this->_sample_type & perf::Sampler::Type::Weight) {
        sample.weight(perf::Weight{ std::uint32_t(*reinterpret_cast<std::uint64_t*>(sample_ptr)) });
        sample_ptr += sizeof(std::uint64_t);
      }
#ifndef NO_PERF_SAMPLE_WEIGHT_STRUCT
      else if (this->_sample_type & perf::Sampler::Type::WeightStruct) {
        const auto weight_struct = *reinterpret_cast<perf_sample_weight*>(sample_ptr);
        sample.weight(perf::Weight{ weight_struct.var1_dw, weight_struct.var2_w, weight_struct.var3_w });

        sample_ptr += sizeof(perf_sample_weight);
      }
#endif

      if (this->_sample_type & perf::Sampler::Type::DataSource) {
        sample.data_src(perf::DataSource{ *reinterpret_cast<std::uint64_t*>(sample_ptr) });
        sample_ptr += sizeof(std::uint64_t);
      }

      if (this->_sample_type & perf::Sampler::KernelRegisters) {
        const auto abi = *reinterpret_cast<std::uint64_t*>(sample_ptr);
        sample.kernel_registers_abi(abi);
        sample_ptr += sizeof(std::uint64_t);

        const auto count_kernel_registers = this->_config.kernel_registers().size();
        if (count_kernel_registers > 0U) {
          auto kernel_registers = std::vector<std::uint64_t>{};
          kernel_registers.reserve(count_kernel_registers);

          const auto* perf_kernel_registers = reinterpret_cast<std::uint64_t*>(sample_ptr);
          for (auto register_id = 0U; register_id < count_kernel_registers; ++register_id) {
            kernel_registers.push_back(perf_kernel_registers[register_id]);
          }

          sample.user_registers(std::move(kernel_registers));

          sample_ptr += sizeof(std::uint64_t) * count_kernel_registers;
        }
      }

      if (this->_sample_type & perf::Sampler::Type::PhysicalMemAddress) {
        sample.physical_memory_address(*reinterpret_cast<std::uint64_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint64_t);
      }

      if (this->_sample_type & perf::Sampler::Type::DataPageSize) {
        sample.data_page_size(*reinterpret_cast<std::uint64_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint64_t);
      }

      if (this->_sample_type & perf::Sampler::Type::CodePageSize) {
        sample.code_page_size(*reinterpret_cast<std::uint64_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint64_t);
      }

      result.push_back(sample);
    }

    /// Go to the next sample.
    iterator += event_header->size;
  }

  return result;
}
