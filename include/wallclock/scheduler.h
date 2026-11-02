#pragma once
#include <ctime>
#include <functional>
#include <string>
namespace wc {
enum class Wake { deadline, environment, visibility, stop };
struct LoopHost {
    std::function<std::time_t()> clock;
    std::function<Wake(std::time_t)> wait;
    std::function<bool()> visible;
    std::function<void(const std::string&)> log;
    // Optional for legacy/test hosts; production supplies monotonic milliseconds.
    std::function<long long()> monotonic_ms;
};
using Update = std::function<std::time_t(std::time_t,bool)>;
int retry_seconds(unsigned failures);
void run_loop(const Update&,const LoopHost&,unsigned max_updates=0);
}
