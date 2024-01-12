#include <perfcpp/sampler.h>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

perf::Sampler::Sampler(const perf::CounterDefinition &counter_list, std::string &&counter_name, const std::uint64_t type, const std::uint64_t frequency, perf::Config config)
    : _counter_definitions(counter_list), _config(config), _sample_config(type, frequency)
{
    if (!this->_counter_definitions.is_metric(counter_name))    /// Metrics are not (yet) supported.
    {
        /// Try to set the counter, if the name refers to a counter.
        if (auto counter_config = this->_counter_definitions.counter(counter_name); counter_config.has_value())
        {
            this->_counter = Counter{counter_config.value()};
        }
    }
}

bool perf::Sampler::open()
{
    if (!this->_counter.has_value())
    {
        return false;
    }

    auto& perf_event = this->_counter->event_attribute();
    std::memset(&perf_event, 0, sizeof(perf_event_attr));
    perf_event.type = this->_counter->type();
    perf_event.size = sizeof(perf_event_attr);
    perf_event.config = this->_counter->event_id();
    perf_event.disabled = true;
    perf_event.inherit = static_cast<std::int32_t>(this->_config.is_include_child_threads());
    perf_event.exclude_kernel = static_cast<std::int32_t>(!this->_config.is_include_kernel());
    perf_event.exclude_user = static_cast<std::int32_t>(!this->_config.is_include_user());
    perf_event.exclude_hv = static_cast<std::int32_t>(!this->_config.is_include_hypervisor());
    perf_event.exclude_idle = static_cast<std::int32_t>(!this->_config.is_include_idle());

    perf_event.sample_type = this->_sample_config.first;
    perf_event.sample_freq = this->_sample_config.second;
    perf_event.freq = 1U;

    perf_event.mmap = 1U;

    perf_event.precise_ip = this->_config.precise_ip();

    /// Open the counter.
    const std::int32_t file_descriptor = syscall(__NR_perf_event_open, &perf_event, 0, -1, -1, 0);
    if (file_descriptor < 0)
    {
        this->_last_error = errno;
        return false;
    }
    this->_counter->file_descriptor(file_descriptor);

    /// Open the mapped buffer.
    this->_buffer = ::mmap(NULL, this->_config.buffer_pages() * 4096U, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
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
        ::ioctl(this->_counter->file_descriptor(), PERF_EVENT_IOC_RESET, 0);
        ::ioctl(this->_counter->file_descriptor(), PERF_EVENT_IOC_ENABLE, 0);
        return true;
    }

    return false;
}

void perf::Sampler::stop()
{
    ::ioctl(this->_counter->file_descriptor(), PERF_EVENT_IOC_DISABLE, 0);
}

void perf::Sampler::close()
{
    ::munmap(this->_buffer, this->_config.buffer_pages() * 4096U);
    ::close(this->_counter->file_descriptor());
}

#include <iostream>
void perf::Sampler::for_each_sample(std::function<void(void*)> &&callback)
{
    auto *mmap_page = reinterpret_cast<perf_event_mmap_page *>(this->_buffer);

    /// When the ringbuffer is empty or already read, there is nothing to do.
    if (mmap_page->data_tail >= mmap_page->data_head)
    {
        return;
    }

    /// The buffer starts at page 1 (from 0).
    auto iterator = std::uintptr_t(this->_buffer) + 4096U;

    /// data_head is the size (in bytes) of the samples.
    const auto end = iterator + mmap_page->data_head;

    while (iterator < end)
    {
        auto *event_header = reinterpret_cast<perf_event_header *>(iterator);

        if (event_header->type == PERF_RECORD_SAMPLE && static_cast<bool>(event_header->misc & PERF_RECORD_MISC_USER))
        {
            callback(reinterpret_cast<void*>(event_header + 1U));
        }

        /// Go to the next sample.
        iterator += event_header->size;
    }
}