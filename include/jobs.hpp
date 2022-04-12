#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <vector>

namespace jobs {
struct Event {
    std::uint64_t id;
    std::size_t worker;
    double start_us, end_us;
};
class Scheduler {
    struct Job;
    struct State;
public:
    class Handle {
        std::shared_ptr<Job> job_;
        explicit Handle(std::shared_ptr<Job> job) : job_(std::move(job)) {}
        friend class Scheduler;
    public:
        void get() const;
    };
    explicit Scheduler(std::size_t workers, std::size_t trace_capacity = 0);
    ~Scheduler();
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Handle submit(std::function<void()> work, std::span<const Handle> after = {});
    void shutdown();
    std::vector<Event> trace() const;
    std::uint64_t steal_count() const noexcept;
private:
    std::unique_ptr<State> state_;
};
}
