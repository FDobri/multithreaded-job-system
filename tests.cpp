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
