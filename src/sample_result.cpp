#include <perfcpp/sample_result.h>
#include <algorithm>

void
perf::SampleResult::filter(std::function<bool(const Sample&)> filter)
{
  this->_samples.erase(std::remove_if(this->_samples.begin(),
                               this->_samples.end(),
                               [&filter](const auto& sample) {
                                 return filter(sample) == false;
                               }),
                this->_samples.end());
}