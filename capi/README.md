# GHOST C API

This directory exposes a stable C ABI over the C++ GHOST model builder. It is
the native boundary used by language bindings such as GHOST.jl.

The current interface supports bounded integer domains, starting points,
linear equality and inequality constraints, `AllDifferent`, linear objectives,
parallel runs, and the main local-search parameters.

The callback extension reports ABI version `0x00010000` through
`ghost_c_api_version()`. It supports error constraints and objective functions
implemented by the calling language. Callback values must be finite, and
constraint errors must be nonnegative. A callback reports failure by returning a
negative status; exceptions must never cross the C boundary. The caller owns
the userdata until it destroys the session, and must keep it alive throughout
every solve. Values are supplied in constraint scope order in a reusable buffer
which must not be retained by the callback.

Callback sessions require `parallel_runs=false` and `number_threads=1` explicitly.
They are evaluated on the calling thread. A Julia binding can run independent
sessions on Julia-managed threads, each with its own callback state and ICN
workspace. Native C++ worker threads are reserved for models without language
callbacks. The ABI rejects unsupported parallel callback options before solving.

This ABI is carried on top of upstream `develop` at
`37bbfdf612af229cf9fbb9688995cae268c53a64` (2026-09-29), rather than an older
release snapshot. The Julia frontend remains a JuMP/MOI optimizer.

The callback test sources are prepared; local execution of this extension is
pending resource availability. The older built-in ABI tests do not qualify the
new callback extension.

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
