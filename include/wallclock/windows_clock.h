#pragma once
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <chrono>
#include <stdexcept>
namespace wc {
// Old MinGW's steady_clock aliases system_clock. Use the Windows interval
// counter explicitly so clock corrections cannot corrupt uptime/duration data.
struct WindowsClock {
    using rep=long long;
    using period=std::nano;
    using duration=std::chrono::nanoseconds;
    using time_point=std::chrono::time_point<WindowsClock>;
    static constexpr bool is_steady=true;
    static time_point now() {
        static const long long frequency=[](){LARGE_INTEGER f;if(!QueryPerformanceFrequency(&f) || f.QuadPart<=0) throw std::runtime_error("Performance counter unavailable");return f.QuadPart;}();
        LARGE_INTEGER ticks;if(!QueryPerformanceCounter(&ticks)) throw std::runtime_error("Performance counter failed");
        return time_point(duration((ticks.QuadPart/frequency)*1000000000LL+(ticks.QuadPart%frequency)*1000000000LL/frequency));
    }
};
}
