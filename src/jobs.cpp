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

}
