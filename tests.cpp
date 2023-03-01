#include "jobs.hpp"
#include <array>
#include <atomic>
#include <gtest/gtest.h>

TEST(Scheduler, DependencyDiamondAndDrain) {
    jobs::Scheduler scheduler(4, 16);
    int parent = 0, a = 0, b = 0, total = 0;
    const std::array root{scheduler.submit([&] { parent = 40; })};
    const std::array leaves{scheduler.submit([&] { a = parent + 1; }, root),
                            scheduler.submit([&] { b = parent + 2; }, root)};
    auto last = scheduler.submit([&] { total = a + b; }, leaves);
    scheduler.shutdown();
    last.get();
    EXPECT_EQ(total, 83);
    EXPECT_EQ(scheduler.trace().size(), 4u);
    EXPECT_THROW(scheduler.submit([] {}), std::logic_error);
}

TEST(Scheduler, SkipsJobsAfterFailure) {
    jobs::Scheduler scheduler(2);
    const std::array parent{scheduler.submit([] { throw std::runtime_error("root"); })};
    std::atomic<bool> ran = false;
    auto child = scheduler.submit([&] { ran = true; }, parent);
    EXPECT_THROW(child.get(), std::runtime_error);
    EXPECT_FALSE(ran.load());
}

TEST(Scheduler, RejectsDependenciesFromOtherSchedulers) {
    jobs::Scheduler a(1), b(1);
    const std::array handle{a.submit([] {})};
    EXPECT_THROW(b.submit([] {}, handle), std::invalid_argument);
}

TEST(Scheduler, RejectsShutdownFromItsOwnWorker) {
    jobs::Scheduler scheduler(1);
    auto task = scheduler.submit([&] { scheduler.shutdown(); });
    EXPECT_THROW(task.get(), std::logic_error);
    scheduler.shutdown();
}
