#include "jobs.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace jobs {
using Clock = std::chrono::steady_clock;
struct Scheduler::Job {
    Job(std::function<void()> action, const std::shared_ptr<int>& owner)
        : work(std::move(action)), result(done.get_future().share()), owner(owner) {}
    std::function<void()> work;
    std::promise<void> done;
    std::shared_future<void> result;
    std::weak_ptr<int> owner;
    std::vector<std::weak_ptr<Job>> dependents;
    std::list<std::shared_ptr<Job>>::iterator blocked_at;
    std::size_t remaining{}, queue{};
    std::uint64_t id{};
    bool completed{};
    std::exception_ptr error, dependency_error;
};

struct Scheduler::State {
    explicit State(std::size_t count, std::size_t trace_capacity) : ready(count) {
        events.reserve(trace_capacity);
    }
    std::mutex mutex, shutdown_mutex;
    std::condition_variable wake;
    std::vector<std::thread> workers;
    std::vector<std::list<std::shared_ptr<Job>>> ready;
    std::list<std::shared_ptr<Job>> blocked;
    std::shared_ptr<int> identity = std::make_shared<int>(0);
    std::vector<Event> events;
    Clock::time_point start = Clock::now();
    std::size_t queued{}, outstanding{}, cursor{};
    std::uint64_t serial{};
    std::atomic<std::uint64_t> stolen{};
    inline static thread_local State* current = nullptr;
    bool closing{};

    void run(std::size_t worker) {
        current = this;
        for (;;) {
            std::unique_lock lock(mutex);
            wake.wait(lock, [&] { return queued || (closing && !outstanding); });
            if (!queued) return;
            std::size_t queue = worker;
            while (ready[queue].empty()) queue = (queue + 1) % ready.size();
            auto& list = ready[queue];
            auto job = queue == worker ? list.back() : list.front();
            if (queue == worker) {
                list.pop_back();
            } else {
                list.pop_front();
                stolen.fetch_add(1, std::memory_order_relaxed);
            }
            --queued;
            lock.unlock();
            const auto begin = Clock::now();
            auto error = job->dependency_error;
            try {
#ifdef JOBS_TRACY
                ZoneScopedN("Job execution");
#endif
                if (!error) job->work();
            } catch (...) { error = std::current_exception(); }
            job->work = nullptr;
            const auto end = Clock::now();
            lock.lock();
            job->completed = true;
            job->error = error;
            for (const auto& reference : job->dependents) {
                if (auto next = reference.lock()) {
                    if (error && !next->dependency_error) next->dependency_error = error;
                    if (--next->remaining == 0) {
                        ready[next->queue].splice(ready[next->queue].end(), blocked, next->blocked_at);
                        ++queued;
                    }
                }
            }
            job->dependents.clear();
            if (events.size() < events.capacity()) {
                using Microseconds = std::chrono::duration<double, std::micro>;
                events.push_back({job->id, worker, Microseconds(begin - start).count(),
                                  Microseconds(end - start).count()});
            }
            if (error) job->done.set_exception(error); else job->done.set_value();
            --outstanding;
            lock.unlock();
            wake.notify_all();
        }
    }
};

Scheduler::Scheduler(std::size_t count, std::size_t trace_capacity) {
    if (!count) throw std::invalid_argument("worker count must be positive");
    state_ = std::make_unique<State>(count, trace_capacity);
    state_->workers.reserve(count);
    try {
        for (std::size_t i = 0; i < count; ++i)
            state_->workers.emplace_back([this, i] { state_->run(i); });
    } catch (...) {
        shutdown();
        throw;
    }
}

Scheduler::~Scheduler() { shutdown(); }
void Scheduler::Handle::get() const { job_->result.get(); }

Scheduler::Handle Scheduler::submit(std::function<void()> work, std::span<const Handle> after) {
    if (!work) throw std::invalid_argument("job has no action");
    auto& state = *state_;
    auto job = std::make_shared<Job>(std::move(work), state.identity);
    {
        std::lock_guard lock(state.mutex);
        if (state.closing) throw std::logic_error("scheduler is closed");
        std::unordered_set<Job*> seen;
        std::vector<Job*> pending;
        for (const auto& handle : after) {
            auto* dependency = handle.job_.get();
            if (!dependency || dependency->owner.lock() != state.identity)
                throw std::invalid_argument("dependency belongs to another scheduler");
            if (!seen.insert(dependency).second) continue;
            if (dependency->completed) {
                if (!job->dependency_error) job->dependency_error = dependency->error;
            } else {
                auto& dependents = dependency->dependents;
                if (dependents.size() == dependents.capacity()) {
                    const auto growth = dependents.size() < dependents.max_size() / 2
                        ? dependents.size() * 2 + 1 : dependents.max_size();
                    if (growth == dependents.size()) throw std::length_error("too many dependents");
                    dependents.reserve(growth);
                }
                pending.push_back(dependency);
            }
        }
        job->id = ++state.serial;
        job->queue = state.cursor;
        state.cursor = (state.cursor + 1) % state.ready.size();
        job->remaining = pending.size();
        if (pending.empty()) {
            state.ready[job->queue].push_back(job);
            ++state.queued;
        } else {
            job->blocked_at = state.blocked.insert(state.blocked.end(), job);
            for (auto* dependency : pending) dependency->dependents.push_back(job);
        }
        ++state.outstanding;
    }
    state.wake.notify_one();
    return Handle(std::move(job));
}

}
