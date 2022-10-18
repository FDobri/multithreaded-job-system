#include "jobs.hpp"
#include "work.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

int main() {
    constexpr std::size_t count = 1024;
    using Clock = std::chrono::steady_clock;
    auto elapsed = [](auto start) {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    };
    auto start = Clock::now();
    std::uint64_t expected = 0;
    for (std::size_t i = 0; i < count; ++i) expected ^= workload(i + 1);
    std::cout << "sequential_ms=" << elapsed(start) << " checksum=" << expected << '\n';
    auto maximum = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned threads = 1; threads <= std::min(maximum, 16u); threads *= 2) {
        jobs::Scheduler scheduler(threads, count + 1);
        std::array<std::uint64_t, count> values{};
        std::vector<jobs::Scheduler::Handle> tasks;
        tasks.reserve(count);
        start = Clock::now();
        for (std::size_t i = 0; i < count; ++i)
            tasks.push_back(scheduler.submit([&, i] { values[i] = workload(i + 1); }));
        std::uint64_t result = 0;
        scheduler.submit([&] { for (auto value : values) result ^= value; }, tasks).get();
        std::cout << "workers=" << threads << " parallel_ms=" << elapsed(start)
                  << " steals=" << scheduler.steal_count() << " checksum=" << result << '\n';
        if (result != expected) return 1;
        if (threads == 4 || (maximum < 4 && threads == 1)) {
            std::ofstream csv("trace.csv");
            csv << "id,worker,start_us,end_us\n";
            for (const auto& e : scheduler.trace())
                csv << e.id << ',' << e.worker << ',' << e.start_us << ',' << e.end_us << '\n';
            if (!csv) return 1;
        }
    }
}
