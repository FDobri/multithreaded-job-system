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
