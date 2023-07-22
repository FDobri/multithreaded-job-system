# Multithreaded Job System

A C++20 library for dependency graphs, worker-local queues and work stealing. Individual job handles carry completion and exceptions.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
./build/jobs_demo
python3 plot_trace.py trace.csv
./build/jobs_bench --benchmark_repetitions=5
```

Use `build/Release/` for the executable path on multi-config generators. Open the generated `trace.svg` for a worker timeline, or configure with `-DJOBS_TRACY=ON` and connect a matching Tracy 0.11.1 profiler while running the demo or benchmark.

## Design

A single mutex covers registration, dependency completion and queue ownership. Workers take from the back of their own queue and steal from the front of another queue. Actions execute outside the mutex. Dependencies refer to earlier submissions, so the graph is acyclic. Failed dependencies propagate their exception to descendants. Shutdown drains accepted work, rejects subsequent submissions and joins workers. Call `shutdown` and block on `Handle::get` from outside the worker pool. Express prerequisites between workers via dependency handles.

The demo compares a direct loop to runs across powers of two in worker count and checks the result with a reduction job. The benchmark runs 1,024 tasks of 8,192 integer mixing iterations per batch, excludes thread creation and records wall-clock time. Use Release and record the CPU, logical core count and compiler with results. The demo enables bounded trace collection and includes it in its timings.
