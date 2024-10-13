#include <algorithm>
#include <asm/unistd.h>
#include <cstring>
#include <exception>
#include <iostream>
#include <perfcpp/sampler.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

perf::Sampler::Sampler(const perf::CounterDefinition& counter_list,
                       std::vector<std::string>&& counter_names,
                       const std::uint64_t type,
                       perf::SampleConfig config)
  : _counter_definitions(counter_list)
  , _config(config)
{
  _values.instruction_pointer(static_cast<bool>(type & PERF_SAMPLE_IP))
    .thread_id(static_cast<bool>(type & PERF_SAMPLE_TID))
    .time(static_cast<bool>(type & PERF_SAMPLE_TIME))
    .logical_memory_address(static_cast<bool>(type & PERF_SAMPLE_ADDR))
    .callchain(static_cast<bool>(type & PERF_SAMPLE_CALLCHAIN))
    .cpu_id(static_cast<bool>(type & PERF_SAMPLE_CPU))
    .weight(static_cast<bool>(type & PERF_SAMPLE_WEIGHT))
    .data_src(static_cast<bool>(type & PERF_SAMPLE_DATA_SRC))
    .identifier(static_cast<bool>(type & PERF_SAMPLE_IDENTIFIER))
    .physical_memory_address(static_cast<bool>(type & PERF_SAMPLE_PHYS_ADDR))
    .data_page_size(static_cast<bool>(type & Type::DataPageSize))
    .code_page_size(static_cast<bool>(type & Type::CodePageSize))
    .weight_struct(static_cast<bool>(type & Type::WeightStruct));

  if (_values.is_set(PERF_SAMPLE_CALLCHAIN)) {
    _values.callchain(this->_config.max_stack());
  }

  if (static_cast<bool>(type & PERF_SAMPLE_BRANCH_STACK)) {
    _values._branch_mask = config.branch_type();
    _values.set(PERF_SAMPLE_BRANCH_STACK, true);
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_USER)) {
    _values.user_registers(config.user_registers());
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_INTR)) {
    _values.kernel_registers(config.kernel_registers());
  }

  /// Extract trigger(s) (the first counter and the second, if the first is an aux counter)
  /// and counter names that will be added to samples.
  bool has_auxiliary = false;
  auto trigger_names = std::vector<std::string>{};
  auto read_counter_names = std::vector<std::string>{};
  for (auto counter_id = 0U; counter_id < counter_names.size(); ++counter_id) {
    auto& counter_name = counter_names[counter_id];
    if (!this->_counter_definitions.is_metric(counter_name)) /// Metrics are not (yet) supported.
    {
      if (auto counter_config = this->_counter_definitions.counter(counter_name); counter_config.has_value()) {
        if (counter_id == 0U && counter_config->second.is_auxiliary()) {
          has_auxiliary = true;
        }
        if ((has_auxiliary && counter_id < 2U) || counter_id < 1U) {
          trigger_names.push_back(std::move(counter_name));
        } else {
          read_counter_names.emplace_back(std::move(counter_name));
        }
      }
    }
  }

  /// Add trigger and counters.
  this->trigger(std::move(trigger_names));
  if (!read_counter_names.empty()) {
    this->_values.counter(std::move(read_counter_names));
  }
}

perf::Sampler&
perf::Sampler::trigger(std::vector<std::vector<std::string>>&& triggers)
{
  auto triggers_with_precision = std::vector<std::vector<std::pair<std::string, Precision>>>{};
  triggers_with_precision.reserve(triggers.size());

  for (auto& trigger_names : triggers) {
    auto trigger_with_precision = std::vector<std::pair<std::string, Precision>>{};

    trigger_with_precision.reserve(trigger_names.size());
    for (auto& trigger_name : trigger_names) {
      trigger_with_precision.emplace_back(std::move(trigger_name), Precision::Unspecified);
    }

    triggers_with_precision.push_back(std::move(trigger_with_precision));
  }

  return this->trigger(std::move(triggers_with_precision));
}

perf::Sampler&
perf::Sampler::trigger(std::vector<std::vector<std::pair<std::string, Precision>>>&& triggers)
{
  this->_trigger_names.reserve(triggers.size());

  for (auto& trigger_group : triggers) {
    auto trigger_group_references = std::vector<std::pair<std::string_view, Precision>>{};
    for (auto& counter_name_and_precision : trigger_group) {
      const auto& trigger_name = std::get<0>(counter_name_and_precision);

      if (!this->_counter_definitions.is_metric(trigger_name)) {
        if (auto counter_config = this->_counter_definitions.counter(trigger_name); counter_config.has_value()) {
          trigger_group_references.emplace_back(std::get<0>(counter_config.value()),
                                                std::get<1>(counter_name_and_precision));
        } else {
          throw std::runtime_error{ std::string{ "Cannot find counter '" }.append(trigger_name).append("'.") };
        }
      } else {
        throw std::runtime_error{ std::string{ "Counter '" }
                                    .append(trigger_name)
                                    .append("' seems to be a metric. Metrics are not supported as triggers.") };
      }
    }
    this->_trigger_names.push_back(std::move(trigger_group_references));
  }

  return *this;
}

void
perf::Sampler::open()
{
  /// Do not open again, if the sampler was already opened.
  if (std::exchange(this->_is_opened, true)) {
    return;
  }

  /// Build the groups from triggers + counters from values.
  for (const auto& trigger_group : this->_trigger_names) {
    auto group = Group{};

    if (this->_values.is_set(PERF_SAMPLE_READ)) {
      /// Add storage for counters, if the user requested to record counter values.
      this->_counter_names.emplace_back();
    }

    /// Add the trigger(s) to the group.
    for (const auto trigger : trigger_group) {
      if (auto counter_config = this->_counter_definitions.counter(std::get<0>(trigger)); counter_config.has_value()) {
        const auto precision =
          std::get<1>(trigger) != Precision::Unspecified ? std::get<1>(trigger) : this->_config.precise_ip();
        auto config = std::get<1>(counter_config.value());
        config.precise_ip(static_cast<std::uint8_t>(precision));

        group.add(config);

        if (this->_values.is_set(PERF_SAMPLE_READ)) {
          this->_counter_names.back().push_back(std::get<0>(trigger));
        }
      }
    }

    if (!group.empty()) {
      /// Add possible counters as value to the sample.
      if (this->_values.is_set(PERF_SAMPLE_READ)) {
        this->_counter_names.emplace_back();

        for (const auto& counter_name : this->_values.counters()) {

          /// Validate the counter is not a metric.
          if (!this->_counter_definitions.is_metric(counter_name)) {

            /// Find the counter.
            if (auto counter_config = this->_counter_definitions.counter(counter_name); counter_config.has_value()) {
              /// Add the counter to the group and to the list of counters.
              this->_counter_names.front().push_back(std::get<0>(counter_config.value()));
              group.add(std::get<1>(counter_config.value()));
            } else {
              throw std::runtime_error{ std::string{ "Cannot find counter '" }.append(counter_name).append("'.") };
            }
          } else {
            throw std::runtime_error{ std::string{ "Counter '" }
                                        .append(counter_name)
                                        .append("' seems to be a metric. Metrics are not supported for sampling.") };
          }
        }
      }

      this->_groups.push_back(std::move(group));
    }
  }

  /// Open the groups.
  if (this->_groups.empty()) {
    throw std::runtime_error{ "No trigger for sampling specified." };
  }

  for (auto& group : this->_groups) {
    /// Detect, if the leader is an auxiliary (specifically for Sapphire Rapids).
    const auto is_leader_auxiliary_counter = group.member(0U).is_auxiliary();

    for (auto counter_index = 0U; counter_index < group.size(); ++counter_index) {
      auto& counter = group.member(counter_index);

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
        perf_event.sample_type = this->_values.get();
        perf_event.sample_id_all = 1U;

        if (this->_config.is_frequency()) {
          perf_event.freq = 1U;
          perf_event.sample_freq = this->_config.frequency_or_period();
        } else {
          perf_event.sample_period = this->_config.frequency_or_period();
        }

        if (this->_values.is_set(PERF_SAMPLE_BRANCH_STACK)) {
          perf_event.branch_sample_type = this->_values.branch_mask();
        }

        if (this->_values.is_set(PERF_SAMPLE_CALLCHAIN)) {
          perf_event.sample_max_stack = this->_values.max_call_stack();
        }

        if (this->_values.is_set(PERF_SAMPLE_REGS_USER)) {
          perf_event.sample_regs_user = this->_values.user_registers().mask();
        }

        if (this->_values.is_set(PERF_SAMPLE_REGS_INTR)) {
          perf_event.sample_regs_intr = this->_values.kernel_registers().mask();
        }

        perf_event.context_switch = static_cast<std::uint8_t>(this->_values._is_include_context_switch);
        perf_event.cgroup = this->_values.is_set(PERF_SAMPLE_CGROUP) ? 1U : 0U;
      }

      if (this->_values.is_set(PERF_SAMPLE_READ)) {
        perf_event.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
      }

      const std::int32_t cpu_id =
        this->_config.cpu_id().has_value() ? std::int32_t{ this->_config.cpu_id().value() } : -1;

      /// Open the counter. Try to decrease the precise_ip if the file syscall was not successful, reporting an invalid
      /// argument.
      std::int64_t file_descriptor;
      for (auto precise_ip = std::int32_t{ counter.precise_ip() }; precise_ip > -1; --precise_ip) {
        perf_event.precise_ip = std::uint64_t(precise_ip);

        counter.precise_ip(
          static_cast<std::uint8_t>(precise_ip)); /// Set back to counter in case the counter is debugged and the user
                                                  /// wants to see the precise_ip that worked.

        file_descriptor = ::syscall(
          __NR_perf_event_open, &perf_event, this->_config.process_id(), cpu_id, group.leader_file_descriptor(), 0);

        /// If opening the file descriptor was successfully, we can start the counter.
        /// Otherwise, we will try to decrease the precision and try again.
        if (file_descriptor > -1 || (errno != EINVAL && errno != EOPNOTSUPP)) {
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
        throw std::runtime_error{ std::string{ "Cannot create file descriptor for sampling counter (error no: " }
                                    .append(std::to_string(errno))
                                    .append(").") };
      }
    }

    /// Open the mapped buffer.
    /// If the leader is an "auxiliary" counter (like on Sapphire Rapid), use the second counter instead.
    const auto file_descriptor = is_leader_auxiliary_counter && group.size() > 1U ? group.member(1U).file_descriptor()
                                                                                  : group.leader_file_descriptor();
    auto* buffer = ::mmap(nullptr,
                          this->_config.buffer_pages() * 4096U,
                          PROT_READ,
                          MAP_SHARED,
                          static_cast<std::int32_t>(file_descriptor),
                          0);
    if (buffer == MAP_FAILED) {
      this->_last_error = errno;
      throw std::runtime_error{ "Creating buffer via mmap() failed." };
    }

    if (buffer == nullptr) {
      throw std::runtime_error{ "Created buffer via mmap() is null." };
    }

    this->_buffers.push_back(buffer);
  }
}

bool
perf::Sampler::start()
{
  /// Open the groups.
  this->open();

  /// Start the counters.
  for (const auto& group : this->_groups) {
    ::ioctl(group.leader_file_descriptor(), PERF_EVENT_IOC_RESET, 0);
    ::ioctl(group.leader_file_descriptor(), PERF_EVENT_IOC_ENABLE, 0);
  }

  return true;
}

void
perf::Sampler::stop()
{
  for (const auto& group : this->_groups) {
    ::ioctl(group.leader_file_descriptor(), PERF_EVENT_IOC_DISABLE, 0);
  }
}

void
perf::Sampler::close()
{
  /// Free all buffers and close all groups.
  for (auto i = 0U; i < this->_groups.size(); ++i) {
    if (this->_buffers[i] != nullptr) {
      ::munmap(this->_buffers[i], this->_config.buffer_pages() * 4096U);
    }

    if (this->_groups[i].leader_file_descriptor() > -1) {
      this->_groups[i].close();
    }
  }

  /// Clear all buffers, groups, and counter names
  /// in order to enable opening again.
  this->_buffers.clear();
  this->_groups.clear();
  this->_counter_names.clear();
  this->_is_opened = false;
}

std::vector<perf::Sample>
perf::Sampler::result(const bool sort_by_time) const
{
  auto result = std::vector<Sample>{};
  result.reserve(2048U);

  for (auto group_id = 0U; group_id < this->_groups.size(); ++group_id) {
    auto* buffer = this->_buffers[group_id];
    if (buffer == nullptr) {
      continue;
    }

    auto* mmap_page = reinterpret_cast<perf_event_mmap_page*>(buffer);

    /// When the ringbuffer is empty or already read, there is nothing to do.
    if (mmap_page->data_tail >= mmap_page->data_head) {
      return result;
    }

    /// The buffer starts at page 1 (from 0).
    auto iterator = std::uintptr_t(buffer) + 4096U;

    /// data_head is the size (in bytes) of the samples.
    const auto end = iterator + mmap_page->data_head;

    while (iterator < end) {
      auto* event_header = reinterpret_cast<perf_event_header*>(iterator);

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

      if (event_header->type == PERF_RECORD_SAMPLE) { /// Read "normal" samples.

        sample.is_exact_ip(static_cast<bool>(event_header->misc & PERF_RECORD_MISC_EXACT_IP));

        auto sample_ptr = std::uintptr_t(reinterpret_cast<void*>(event_header + 1U));

        if (this->_values.is_set(PERF_SAMPLE_IDENTIFIER)) {
          sample.sample_id(*reinterpret_cast<std::uint64_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_IP)) {
          sample.instruction_pointer(*reinterpret_cast<std::uintptr_t*>(sample_ptr));
          sample_ptr += sizeof(std::uintptr_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_TID)) {
          sample.process_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint32_t);

          sample.thread_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint32_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_TIME)) {
          sample.timestamp(*reinterpret_cast<std::uint64_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_STREAM_ID)) {
          sample.stream_id(*reinterpret_cast<std::uint64_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_ADDR)) {
          sample.logical_memory_address(*reinterpret_cast<std::uint64_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_CPU)) {
          sample.cpu_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_PERIOD)) {
          sample.period(*reinterpret_cast<std::uint64_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_READ)) {
          auto* read_format = reinterpret_cast<Sampler::read_format*>(sample_ptr);
          const auto count_counter_values = read_format->count_members;

          if (count_counter_values == this->_groups[group_id].size()) {
            auto counter_values = std::vector<std::pair<std::string_view, double>>{};
            for (auto counter_id = 0U; counter_id < this->_groups[group_id].size(); ++counter_id) {
              counter_values.emplace_back(this->_counter_names[group_id][counter_id],
                                          double(read_format->values[counter_id].value));
            }

            sample.counter_result(CounterResult{ std::move(counter_values) });
          }

          sample_ptr +=
            sizeof(Sampler::read_format::count_members) + count_counter_values * sizeof(Sampler::read_format::value);
        }

        if (this->_values.is_set(PERF_SAMPLE_CALLCHAIN)) {
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

        if (this->_values.is_set(PERF_SAMPLE_RAW)) {
          const auto raw_data_size = (*reinterpret_cast<std::uint32_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint32_t);

          const auto* raw_sample_data = reinterpret_cast<char*>(sample_ptr);

          auto raw_data = std::vector<char>(std::size_t{ raw_data_size }, '\0');
          for (auto i = 0U; i < raw_data_size; ++i) {
            raw_data[i] = raw_sample_data[i];
          }

          sample.raw(std::move(raw_data));

          sample_ptr += raw_data_size * sizeof(char);
        }

        if (this->_values.is_set(PERF_SAMPLE_BRANCH_STACK)) {
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

        if (this->_values.is_set(PERF_SAMPLE_REGS_USER)) {
          const auto abi = *reinterpret_cast<std::uint64_t*>(sample_ptr);
          sample.user_registers_abi(abi);
          sample_ptr += sizeof(std::uint64_t);

          const auto count_user_registers = this->_values.user_registers().size();
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

        if (this->_values.is_set(PERF_SAMPLE_WEIGHT)) {
          sample.weight(perf::Weight{ std::uint32_t(*reinterpret_cast<std::uint64_t*>(sample_ptr)) });
          sample_ptr += sizeof(std::uint64_t);
        }

#ifndef NO_PERF_SAMPLE_WEIGHT_STRUCT
        else if (this->_values.is_set(PERF_SAMPLE_WEIGHT_STRUCT)) {
          const auto weight_struct = *reinterpret_cast<perf_sample_weight*>(sample_ptr);
          sample.weight(perf::Weight{ weight_struct.var1_dw, weight_struct.var2_w, weight_struct.var3_w });

          sample_ptr += sizeof(perf_sample_weight);
        }
#endif

        if (this->_values.is_set(PERF_SAMPLE_DATA_SRC)) {
          sample.data_src(perf::DataSource{ *reinterpret_cast<std::uint64_t*>(sample_ptr) });
          sample_ptr += sizeof(std::uint64_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_REGS_INTR)) {
          const auto abi = *reinterpret_cast<std::uint64_t*>(sample_ptr);
          sample.kernel_registers_abi(abi);
          sample_ptr += sizeof(std::uint64_t);

          const auto count_kernel_registers = this->_values.kernel_registers().size();
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

        if (this->_values.is_set(PERF_SAMPLE_PHYS_ADDR)) {
          sample.physical_memory_address(*reinterpret_cast<std::uint64_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_CGROUP)) {
          sample.cgroup_id(*reinterpret_cast<std::uint64_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }

#ifndef NO_PERF_SAMPLE_DATA_PAGE_SIZE
        if (this->_values.is_set(PERF_SAMPLE_DATA_PAGE_SIZE)) {
          sample.data_page_size(*reinterpret_cast<std::uint64_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }
#endif

#ifndef NO_PERF_SAMPLE_CODE_PAGE_SIZE
        if (this->_values.is_set(PERF_SAMPLE_CODE_PAGE_SIZE)) {
          sample.code_page_size(*reinterpret_cast<std::uint64_t*>(sample_ptr));
        }
#endif

        result.push_back(sample);
      } else if (event_header->type == PERF_RECORD_LOST_SAMPLES) { /// Read lost samples.

        auto sample_ptr = std::uintptr_t(reinterpret_cast<void*>(event_header + 1U));

        sample.count_loss(*reinterpret_cast<std::uint64_t*>(sample_ptr));
        sample_ptr += sizeof(std::uint64_t);

        /// Read sample_id.
        std::ignore = this->read_sample_id(sample_ptr, sample);

        result.push_back(sample);
      } else if (event_header->type == PERF_RECORD_SWITCH) { /// Read context switch.
        const auto is_switch_out = static_cast<bool>(event_header->misc & PERF_RECORD_MISC_SWITCH_OUT);
        const auto is_switch_out_preempt = static_cast<bool>(event_header->misc & PERF_RECORD_MISC_SWITCH_OUT_PREEMPT);

        auto sample_ptr = std::uintptr_t(reinterpret_cast<void*>(event_header + 1U));
        std::ignore = this->read_sample_id(sample_ptr, sample);

        sample.context_switch(ContextSwitch{ is_switch_out, is_switch_out_preempt });

        result.push_back(sample);
      } else if (event_header->type == PERF_RECORD_SWITCH_CPU_WIDE) { /// Read context switch.
        const auto is_switch_out = static_cast<bool>(event_header->misc & PERF_RECORD_MISC_SWITCH_OUT);
        const auto is_switch_out_preempt = static_cast<bool>(event_header->misc & PERF_RECORD_MISC_SWITCH_OUT_PREEMPT);

        auto sample_ptr = std::uintptr_t(reinterpret_cast<void*>(event_header + 1U));

        const auto process_id = *reinterpret_cast<std::uint32_t*>(sample_ptr);
        sample_ptr += sizeof(std::uint32_t);

        const auto thread_id = *reinterpret_cast<std::uint32_t*>(sample_ptr);
        sample_ptr += sizeof(std::uint32_t);

        std::ignore = this->read_sample_id(sample_ptr, sample);

        sample.context_switch(ContextSwitch{ is_switch_out, is_switch_out_preempt, process_id, thread_id });

        result.push_back(sample);
      } else if (event_header->type == PERF_RECORD_CGROUP) { /// Read cgroup samples.

        auto sample_ptr = std::uintptr_t(reinterpret_cast<void*>(event_header + 1U));

        const auto cgroup_id = *reinterpret_cast<std::uint64_t*>(sample_ptr);
        sample_ptr += sizeof(std::uint64_t);

        auto* path = reinterpret_cast<char*>(sample_ptr);

        sample.cgroup(CGroup{ cgroup_id, std::string{ path } });

        result.push_back(sample);
      } else if ((event_header->type == PERF_RECORD_THROTTLE || event_header->type == PERF_RECORD_UNTHROTTLE) &&
                 this->_values._is_include_throttle) { /// Read (un-) throttle samples.
        auto sample_ptr = std::uintptr_t(reinterpret_cast<void*>(event_header + 1U));

        if (this->_values.is_set(PERF_SAMPLE_TIME)) {
          sample.timestamp(*reinterpret_cast<std::uint64_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }

        if (this->_values.is_set(PERF_SAMPLE_STREAM_ID)) {
          sample.stream_id(*reinterpret_cast<std::uint64_t*>(sample_ptr));
          sample_ptr += sizeof(std::uint64_t);
        }

        std::ignore = this->read_sample_id(sample_ptr, sample);

        sample.throttle(Throttle{ event_header->type == PERF_RECORD_THROTTLE });

        result.push_back(sample);
      }

      /// Go to the next sample.
      iterator += event_header->size;
    }
  }

  /// Sort the samples if requested and we can sort by time.
  if (this->_values.is_set(PERF_SAMPLE_TIME) && sort_by_time) {
    std::sort(result.begin(), result.end(), SampleTimestampComparator{});
  }

  return result;
}

std::uintptr_t
perf::Sampler::read_sample_id(std::uintptr_t sample_ptr, perf::Sample& sample) const noexcept
{
  if (this->_values.is_set(PERF_SAMPLE_TID)) {
    sample.process_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
    sample_ptr += sizeof(std::uint32_t);

    sample.thread_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
    sample_ptr += sizeof(std::uint32_t);
  }

  if (this->_values.is_set(PERF_SAMPLE_TIME)) {
    sample.timestamp(*reinterpret_cast<std::uint64_t*>(sample_ptr));
    sample_ptr += sizeof(std::uint64_t);
  }

  if (this->_values.is_set(PERF_SAMPLE_STREAM_ID)) {
    sample.stream_id(*reinterpret_cast<std::uint64_t*>(sample_ptr));
    sample_ptr += sizeof(std::uint64_t);
  }

  if (this->_values.is_set(PERF_SAMPLE_CPU)) {
    sample.cpu_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
    sample_ptr += sizeof(std::uint64_t);
  }

  if (this->_values.is_set(PERF_SAMPLE_IDENTIFIER)) {
    sample.id(*reinterpret_cast<std::uint64_t*>(sample_ptr));
    sample_ptr += sizeof(std::uint64_t);
  }

  return sample_ptr;
}

std::vector<perf::Sample>
perf::MultiSamplerBase::result(const std::vector<Sampler>& sampler, bool sort_by_time)
{
  if (!sampler.empty()) {
    auto result = sampler.front().result();

    for (auto i = 1U; i < sampler.size(); ++i) {
      /// Only sort if all samplers recorded the timestamp.
      sort_by_time &= sampler[i]._values.is_set(PERF_SAMPLE_TIME);

      auto temp_result = sampler[i].result();
      std::move(temp_result.begin(), temp_result.end(), std::back_inserter(result));
    }

    if (sort_by_time) {
      std::sort(result.begin(), result.end(), SampleTimestampComparator{});
    }

    return result;
  }

  return std::vector<perf::Sample>{};
}

void
perf::MultiSamplerBase::trigger(std::vector<Sampler>& samplers, std::vector<std::vector<std::string>>&& trigger_names)
{
  for (auto sampler_id = 0U; sampler_id < samplers.size(); ++sampler_id) {
    if (sampler_id < samplers.size() - 1U) {
      samplers[sampler_id].trigger(std::vector<std::vector<std::string>>{ trigger_names });
    } else {
      samplers[sampler_id].trigger(std::move(trigger_names));
    }
  }
}

void
perf::MultiSamplerBase::trigger(std::vector<Sampler>& samplers,
                                std::vector<std::vector<std::pair<std::string, Precision>>>&& triggers)
{
  for (auto sampler_id = 0U; sampler_id < samplers.size(); ++sampler_id) {
    if (sampler_id < samplers.size() - 1U) {
      samplers[sampler_id].trigger(std::vector<std::vector<std::pair<std::string, Precision>>>{ triggers });
    } else {
      samplers[sampler_id].trigger(std::move(triggers));
    }
  }
}

void
perf::MultiSamplerBase::open(perf::Sampler& sampler, const perf::SampleConfig config)
{
  sampler._values = _values;
  sampler._config = config;

  sampler.open();
}

void
perf::MultiSamplerBase::start(perf::Sampler& sampler, const perf::SampleConfig config)
{
  sampler._values = _values;
  sampler._config = config;

  std::ignore = sampler.start();
}

perf::MultiThreadSampler::MultiThreadSampler(const perf::CounterDefinition& counter_list,
                                             std::vector<std::string>&& counter_names,
                                             const std::uint64_t type,
                                             const std::uint16_t num_threads,
                                             const perf::SampleConfig config)
  : MultiSamplerBase(config)
{
  /// Setup new values field (will be transferred to samplers at start).
  _values.instruction_pointer(static_cast<bool>(type & PERF_SAMPLE_IP))
    .thread_id(static_cast<bool>(type & PERF_SAMPLE_TID))
    .time(static_cast<bool>(type & PERF_SAMPLE_TIME))
    .logical_memory_address(static_cast<bool>(type & PERF_SAMPLE_ADDR))
    .callchain(static_cast<bool>(type & PERF_SAMPLE_CALLCHAIN))
    .cpu_id(static_cast<bool>(type & PERF_SAMPLE_CPU))
    .weight(static_cast<bool>(type & PERF_SAMPLE_WEIGHT))
    .data_src(static_cast<bool>(type & PERF_SAMPLE_DATA_SRC))
    .identifier(static_cast<bool>(type & PERF_SAMPLE_IDENTIFIER))
    .physical_memory_address(static_cast<bool>(type & PERF_SAMPLE_PHYS_ADDR))
    .data_page_size(static_cast<bool>(type & Sampler::Type::DataPageSize))
    .code_page_size(static_cast<bool>(type & Sampler::Type::CodePageSize))
    .weight_struct(static_cast<bool>(type & Sampler::Type::WeightStruct));

  if (static_cast<bool>(type & PERF_SAMPLE_BRANCH_STACK)) {
    _values._branch_mask = config.branch_type();
    _values.set(PERF_SAMPLE_BRANCH_STACK, true);
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_USER)) {
    _values.user_registers(config.user_registers());
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_INTR)) {
    _values.kernel_registers(config.kernel_registers());
  }

  /// Extract trigger(s) (the first counter and the second, if the first is an aux counter)
  /// and counter names that will be added to samples.
  bool has_auxiliary = false;
  auto trigger_names = std::vector<std::string>{};
  auto read_counter_names = std::vector<std::string>{};
  for (auto counter_id = 0U; counter_id < counter_names.size(); ++counter_id) {
    auto& counter_name = counter_names[counter_id];
    if (!counter_list.is_metric(counter_name)) /// Metrics are not (yet) supported.
    {
      if (auto counter_config = counter_list.counter(counter_name); counter_config.has_value()) {
        if (counter_id == 0U && counter_config->second.is_auxiliary()) {
          has_auxiliary = true;
        }
        if ((has_auxiliary && counter_id < 2U) || counter_id < 1U) {
          trigger_names.push_back(std::move(counter_name));
        } else {
          read_counter_names.emplace_back(std::move(counter_name));
        }
      }
    }
  }

  /// Add trigger and counters.
  if (!read_counter_names.empty()) {
    this->_values.counter(std::move(read_counter_names));
  }

  /// Create thread-local samplers without config (will be set when starting).
  for (auto thread_id = 0U; thread_id < num_threads; ++thread_id) {
    auto& thread_sampler = this->_thread_local_samplers.emplace_back(counter_list);
    if (!trigger_names.empty()) {
      thread_sampler.trigger(std::vector<std::vector<std::string>>{ trigger_names });
    }
  }
}

perf::MultiThreadSampler::MultiThreadSampler(const perf::CounterDefinition& counter_list,
                                             const std::uint16_t num_threads,
                                             const perf::SampleConfig config)
  : MultiSamplerBase(config)
{
  /// Create thread-local samplers without config (will be set when starting).
  for (auto thread_id = 0U; thread_id < num_threads; ++thread_id) {
    this->_thread_local_samplers.emplace_back(counter_list);
  }
}

perf::MultiCoreSampler::MultiCoreSampler(const perf::CounterDefinition& counter_list,
                                         std::vector<std::string>&& counter_names,
                                         const std::uint64_t type,
                                         std::vector<std::uint16_t>&& core_ids,
                                         perf::SampleConfig config)
  : MultiSamplerBase(config)
  , _core_ids(std::move(core_ids))
{
  /// Record all processes on the CPUs.
  _config.process_id(-1);

  /// Setup new values field (will be transferred to samplers at start).
  _values.instruction_pointer(static_cast<bool>(type & PERF_SAMPLE_IP))
    .thread_id(static_cast<bool>(type & PERF_SAMPLE_TID))
    .time(static_cast<bool>(type & PERF_SAMPLE_TIME))
    .logical_memory_address(static_cast<bool>(type & PERF_SAMPLE_ADDR))
    .callchain(static_cast<bool>(type & PERF_SAMPLE_CALLCHAIN))
    .cpu_id(static_cast<bool>(type & PERF_SAMPLE_CPU))
    .weight(static_cast<bool>(type & PERF_SAMPLE_WEIGHT))
    .data_src(static_cast<bool>(type & PERF_SAMPLE_DATA_SRC))
    .identifier(static_cast<bool>(type & PERF_SAMPLE_IDENTIFIER))
    .physical_memory_address(static_cast<bool>(type & PERF_SAMPLE_PHYS_ADDR))
    .data_page_size(static_cast<bool>(type & Sampler::Type::DataPageSize))
    .code_page_size(static_cast<bool>(type & Sampler::Type::CodePageSize))
    .weight_struct(static_cast<bool>(type & Sampler::Type::WeightStruct));

  if (static_cast<bool>(type & PERF_SAMPLE_BRANCH_STACK)) {
    _values._branch_mask = config.branch_type();
    _values.set(PERF_SAMPLE_BRANCH_STACK, true);
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_USER)) {
    _values.user_registers(config.user_registers());
  }

  if (static_cast<bool>(type & PERF_SAMPLE_REGS_INTR)) {
    _values.kernel_registers(config.kernel_registers());
  }

  /// Extract trigger(s) (the first counter and the second, if the first is an aux counter)
  /// and counter names that will be added to samples.
  bool has_auxiliary = false;
  auto trigger_names = std::vector<std::string>{};
  auto read_counter_names = std::vector<std::string>{};
  for (auto counter_id = 0U; counter_id < counter_names.size(); ++counter_id) {
    auto& counter_name = counter_names[counter_id];
    if (!counter_list.is_metric(counter_name)) /// Metrics are not (yet) supported.
    {
      if (auto counter_config = counter_list.counter(counter_name); counter_config.has_value()) {
        if (counter_id == 0U && counter_config->second.is_auxiliary()) {
          has_auxiliary = true;
        }
        if ((has_auxiliary && counter_id < 2U) || counter_id < 1U) {
          trigger_names.push_back(std::move(counter_name));
        } else {
          read_counter_names.emplace_back(std::move(counter_name));
        }
      }
    }
  }

  /// Add trigger and counters.
  if (!read_counter_names.empty()) {
    this->_values.counter(std::move(read_counter_names));
  }

  /// Create thread-local samplers without config (will be set when starting).
  for (const auto _ : this->_core_ids) {
    auto& cpu_sampler = this->_core_local_samplers.emplace_back(counter_list);
    if (!trigger_names.empty()) {
      cpu_sampler.trigger(std::vector<std::vector<std::string>>{ trigger_names });
    }
  }
}

perf::MultiCoreSampler::MultiCoreSampler(const perf::CounterDefinition& counter_list,
                                         std::vector<std::uint16_t>&& core_ids,
                                         perf::SampleConfig config)
  : MultiSamplerBase(config)
  , _core_ids(std::move(core_ids))
{
  /// Record all processes on the CPUs.
  _config.process_id(-1);

  /// Create thread-local samplers without config (will be set when starting).
  for (const auto _ : this->_core_ids) {
    this->_core_local_samplers.emplace_back(counter_list);
  }
}

void
perf::MultiCoreSampler::open()
{
  for (auto sampler_id = 0U; sampler_id < this->_core_ids.size(); ++sampler_id) {
    auto config = this->_config;
    config.cpu_id(this->_core_ids[sampler_id]);
    MultiSamplerBase::open(this->_core_local_samplers[sampler_id], config);
  }
}

bool
perf::MultiCoreSampler::start()
{
  for (auto sampler_id = 0U; sampler_id < this->_core_ids.size(); ++sampler_id) {
    auto config = this->_config;
    config.cpu_id(this->_core_ids[sampler_id]);
    MultiSamplerBase::start(this->_core_local_samplers[sampler_id], config);
  }

  return true;
}