#include <perfcpp/sampler.h>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

perf::Sampler::Sampler(const perf::CounterDefinition &counter_list, std::vector<std::string> &&counter_names, const std::uint64_t type, perf::SampleConfig config)
       : _counter_definitions(counter_list), _config(config), _sample_type(type)
{
    for (const auto& counter_name : counter_names)
    {
        if (!this->_counter_definitions.is_metric(counter_name))    /// Metrics are not (yet) supported.
        {
            /// Try to set the counter, if the name refers to a counter.
            if (auto counter_config = this->_counter_definitions.counter(counter_name); counter_config.has_value())
            {
                this->_group.add(counter_config.value());
            }
        }
    }
}


bool perf::Sampler::open()
{
    if (this->_group.empty())
    {
        return false;
    }

    auto leader_file_descriptor = -1;
    for (auto counter_index = 0U; counter_index < this->_group.size(); ++counter_index)
    {
        auto &counter = this->_group.member(counter_index);
        const auto is_leader = counter_index == 0U;

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


        if (is_leader)
        {
            perf_event.sample_type = this->_sample_type;

            if (this->_config.is_frequency())
            {
                perf_event.freq = 1U;
                perf_event.sample_freq = this->_config.frequency_or_period();
            }
            else
            {
                perf_event.sample_period = this->_config.frequency_or_period();
            }

            if (this->_sample_type & Sampler::Type::BranchStack)
            {
                perf_event.branch_sample_type = PERF_SAMPLE_BRANCH_ANY;
//                perf_event.mmap2 = 1U;
//                perf_event.comm_exec = 1U;
//                perf_event.ksymbol = 1U;
            }

            perf_event.mmap = 1U;
            perf_event.precise_ip = this->_config.precise_ip();
        }

        if (this->_sample_type & static_cast<std::uint64_t>(Type::CounterValues))
        {
            perf_event.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID | PERF_FORMAT_LOST;
        }

        /// Open the counter.
        const std::int32_t file_descriptor = syscall(__NR_perf_event_open, &perf_event, 0, -1, leader_file_descriptor, 0);
        if (file_descriptor < 0)
        {
            this->_last_error = errno;
            return false;
        }
        counter.file_descriptor(file_descriptor);

        if (is_leader)
        {
            leader_file_descriptor = file_descriptor;
        }
    }

    /// Open the mapped buffer.
    this->_buffer = ::mmap(NULL, this->_config.buffer_pages() * 4096U, PROT_READ | PROT_WRITE, MAP_SHARED, leader_file_descriptor, 0);
    if (this->_buffer == MAP_FAILED)
    {
        this->_last_error = errno;
        return false;
    }

    return this->_buffer != nullptr;
}

bool perf::Sampler::start()
{
    /// Start the counters.
    if (this->open())
    {
        ::ioctl(this->_group.leader_file_descriptor(), PERF_EVENT_IOC_RESET, 0);
        ::ioctl(this->_group.leader_file_descriptor(), PERF_EVENT_IOC_ENABLE, 0);
        return true;
    }

    return false;
}

void perf::Sampler::stop()
{
    ::ioctl(this->_group.leader_file_descriptor(), PERF_EVENT_IOC_DISABLE, 0);
}

void perf::Sampler::close()
{
    ::munmap(this->_buffer, this->_config.buffer_pages() * 4096U);
    ::close(this->_group.leader_file_descriptor());
}

std::vector<perf::Sample> perf::Sampler::result() const
{
    auto result = std::vector<Sample>{};
    auto *mmap_page = reinterpret_cast<perf_event_mmap_page *>(this->_buffer);

    /// When the ringbuffer is empty or already read, there is nothing to do.
    if (mmap_page->data_tail >= mmap_page->data_head)
    {
        return result;
    }

    result.reserve(2048U);

    /// The buffer starts at page 1 (from 0).
    auto iterator = std::uintptr_t(this->_buffer) + 4096U;

    /// data_head is the size (in bytes) of the samples.
    const auto end = iterator + mmap_page->data_head;

    while (iterator < end)
    {
        auto *event_header = reinterpret_cast<perf_event_header *>(iterator);

        if (event_header->type == PERF_RECORD_SAMPLE)
        {
            auto mode = Sample::Mode::Unknown;
            if (static_cast<bool>(event_header->misc & PERF_RECORD_MISC_KERNEL))
            {
                mode = Sample::Mode::Kernel;
            }
            else if (static_cast<bool>(event_header->misc & PERF_RECORD_MISC_USER))
            {
                mode = Sample::Mode::User;
            }
            else if (static_cast<bool>(event_header->misc & PERF_RECORD_MISC_HYPERVISOR))
            {
                mode = Sample::Mode::Hypervisor;
            }
            else if (static_cast<bool>(event_header->misc & PERF_RECORD_MISC_GUEST_KERNEL))
            {
                mode = Sample::Mode::GuestKernel;
            }
            else if (static_cast<bool>(event_header->misc & PERF_RECORD_MISC_GUEST_USER))
            {
                mode = Sample::Mode::GuestUser;
            }

            auto sample = Sample{mode};

            auto sample_ptr = std::uintptr_t(reinterpret_cast<void*>(event_header + 1U));

            if (this->_sample_type & perf::Sampler::Type::Identifier)
            {
                sample.sample_id(*reinterpret_cast<std::uint64_t*>(sample_ptr));
                sample_ptr += sizeof(std::uint64_t);
            }

            if (this->_sample_type & perf::Sampler::Type::InstructionPointer)
            {
                sample.instruction_pointer(*reinterpret_cast<std::uintptr_t*>(sample_ptr));
                sample_ptr += sizeof(std::uintptr_t);
            }

            if (this->_sample_type & perf::Sampler::Type::ThreadId)
            {
                sample.process_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
                sample_ptr += sizeof(std::uint32_t);

                sample.thread_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
                sample_ptr += sizeof(std::uint32_t);
            }

            if (this->_sample_type & perf::Sampler::Type::Timestamp)
            {
                sample.timestamp(*reinterpret_cast<std::uint64_t*>(sample_ptr));
                sample_ptr += sizeof(std::uint64_t);
            }

            if (this->_sample_type & perf::Sampler::Type::LogicalMemAddress)
            {
                sample.logical_memory_address(*reinterpret_cast<std::uint64_t*>(sample_ptr));
                sample_ptr += sizeof(std::uint64_t);
            }

            if (this->_sample_type & perf::Sampler::Type::CPU)
            {
                sample.cpu_id(*reinterpret_cast<std::uint32_t*>(sample_ptr));
                sample_ptr += sizeof(std::uint64_t);
            }

            if (this->_sample_type & perf::Sampler::BranchStack)
            {
                const auto count_branches = *reinterpret_cast<std::uint64_t*>(sample_ptr);
                sample_ptr += sizeof(std::uint64_t);

                if (count_branches > 0U)
                {
                    auto branches = std::vector<Branch>{};
                    branches.reserve(count_branches);

                    auto *sampled_branches = reinterpret_cast<perf_branch_entry*>(sample_ptr);
                    for (auto i = 0U; i < count_branches; ++i)
                    {
                        const auto& branch = sampled_branches[i];
                        branches.emplace_back(branch.from, branch.to, branch.mispred, branch.predicted, branch.in_tx, branch.abort, branch.cycles);
                    }

                    sample.branches(std::move(branches));
                }

                sample_ptr += sizeof(perf_branch_entry) * count_branches;
            }

            if (this->_sample_type & perf::Sampler::Type::Weight)
            {
                sample.weight(*reinterpret_cast<std::uint64_t*>(sample_ptr));
                sample_ptr += sizeof(std::uint64_t);
            }

            if (this->_sample_type & perf::Sampler::Type::WeightStruct)
            {
                sample.weight(reinterpret_cast<perf_sample_weight*>(sample_ptr)->full);
                sample_ptr += sizeof(std::uint64_t);
            }

            if (this->_sample_type & perf::Sampler::Type::DataSource)
            {
                sample.data_src(perf::DataSource{*reinterpret_cast<std::uint64_t*>(sample_ptr)});
                sample_ptr += sizeof(std::uint64_t);
            }

            if (this->_sample_type & perf::Sampler::Type::PhysicalMemAddress)
            {
                sample.physical_memory_address(*reinterpret_cast<std::uint64_t*>(sample_ptr));
            }

            result.push_back(sample);
        }

        /// Go to the next sample.
        iterator += event_header->size;
    }

    return result;
}
