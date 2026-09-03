# GHOST C API

This directory exposes a stable C ABI over the C++ GHOST model builder. It is
the native boundary used by language bindings such as GHOST.jl.

The current interface supports bounded integer domains, starting points,
linear equality and inequality constraints, `AllDifferent`, linear objectives,
parallel runs, and the main local-search parameters.

`ghost_solve` uses GHOST's heuristic `fast_search`. Consequently:

- a feasible satisfaction result is returned as `GHOST_SAT_FOUND`;
- a feasible optimization result is returned as `GHOST_FEASIBLE_FOUND`;
- exhausting the time budget without a solution is `GHOST_TIME_LIMIT`, never
  `GHOST_INFEASIBLE`.

The shared `ghost_c` library statically links the GHOST core so that consumers
load one native library. Build and test it with the root CMake project:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Binary distributions that only need the stable C ABI can avoid installing the
C++ library and headers:

```sh
cmake -S . -B build -DGHOST_C_API_ONLY=ON -DNO_ASAN=ON
cmake --build build --config Release
cmake --install build --prefix /path/to/prefix
```
