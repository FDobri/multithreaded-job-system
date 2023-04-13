#include "jobs.hpp"
#include "work.hpp"
#include <array>
#include <benchmark/benchmark.h>

static void jobs_batch(benchmark::State& state) {
    constexpr std::size_t count = 1024;
    jobs::Scheduler pool(static_cast<std::size_t>(state.range(0) ? state.range(0) : 1));
    std::vector<jobs::Scheduler::Handle> handles;
    handles.reserve(count);
    std::array<std::uint64_t, count> values{};
    for (auto iteration : state) {
        if (state.range(0) == 0) {
            for (std::size_t i = 0; i < count; ++i) values[i] = workload(i + 1);
        } else {
            handles.clear();
            for (std::size_t i = 0; i < count; ++i)
                handles.push_back(pool.submit([&, i] { values[i] = workload(i + 1); }));
            for (const auto& handle : handles) handle.get();
        }
        benchmark::DoNotOptimize(values.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(jobs_batch)->Arg(0)->Arg(1)->Arg(2)->Arg(4)->Arg(8)->Arg(16)->UseRealTime();
