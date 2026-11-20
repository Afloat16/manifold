# Tangent distribution scratch: reproducible allocation comparison

This is supporting evidence for `Afloat16/manifold` branch `perf/tangent-angle-scratch-20261120`.

- Base: `6ca62006abf4579f60c40ee7b6c49b66cc4ae8fc`.
- Patch: `b7bea53819e449f1e1ce9e63c997f932885c3702` (tree `50df49b63a726d8a8b78ef6b6c9735e9ab6b1995`).
- Native GCC 14.2, CMake 4.4, Release, strict warnings, assertions/debug checks, C/Python bindings enabled. Builds ran serially. TBB uses the repository's pinned v2022.3.0 (`f1862f38f83568d96e814e469ab61f88336cc595`).

## Scope and method

The patch keeps the two angle vectors in `DistributeTangents` across sectors of the same call. Serial builds use one pair; TBB builds use invocation-local `tbb::combinable` worker storage. Both vector lengths are reset before each sector, including paths which return early. No tangent arithmetic, ordering, fixed-edge flags, or parallel thresholds change.

The attached C++ probe calls public `SmoothByNormals` and intercepts glibc `malloc` while that call runs. The recorded bytes are cumulative requested allocation traffic, not peak live memory, RSS, or total process memory. Each case warms once and then measures seven calls; CSV reports the median/IQR latency and final sample's allocation totals. Allocation counts include the rest of public smoothing, not just this helper. TBB allocator bookkeeping is included where it reaches malloc.

The cases include spheres at three resolutions, absent/partial normals, a faceted cube, a creased cylinder, a 512-sided high-valence cone, high- then low-valence components, pre-existing tangents, and empty input. For each result the probe serializes `numProp` and all ten MeshGL64 vectors: vertices/properties, triangle indices, merge relations, run indices/IDs/transforms/flags, face IDs, and tangents. Repeated calls are byte-identical; base and patch files compare byte-for-byte in both builds. The input/shared alias is checked unchanged. Every result has `NoError` status.

## Results

| Configuration / input | Base bytes | Patch bytes | Base calls | Patch calls |
| --- | ---: | ---: | ---: | ---: |
| Serial sphere, 2,048 triangles | 2,997,252 | 898,052 | 3,096 | 1,046 |
| Serial sphere, 32,768 triangles | 47,867,268 | 14,310,788 | 49,176 | 16,406 |
| TBB 2-CPU sphere, 32,768 triangles | 48,099,588 | 14,545,164 | 49,178 | 16,411 |
| Serial high/low valence components | 1,529,400 | 464,440 | 1,586 | 546 |
| Serial empty control | 768 | 768 | 3 | 3 |

All 22 case/configuration output payloads match the base; 22 input payloads also match. The hashes and complete comparison summaries are attached. Raw binary payloads can be regenerated with the probe and are not committed here.

Timing varied on the shared host. The largest serial sphere measured 23.02 -> 25.58 ms (IQR 2.25 -> 5.48 ms), while the two-CPU TBB version measured 14.54 -> 13.34 ms (IQR 2.93 -> 2.62 ms). Some smaller cases are also slower. The claim is reduced allocation traffic; these samples do not establish a general latency gain. All timings, including regressions, are in the CSV files.

## Validation

- Serial strict build: 98 targets, including native tests, C and Python bindings; incremental patch build succeeded.
- TBB strict build: 119 targets; incremental patch build succeeded.
- `manifold_test --gtest_filter=Smooth.*:Manifold.*:Properties.*`: 161/161 passed in each configuration (35.48 s serial, 19.36 s TBB), with repository test-main geometry checks enabled.
- 11 public-API probe cases in each configuration, seven repeated results each, complete payload equivalence and unchanged inputs.
- `clang-format --dry-run -Werror src/smoothing.cpp` and `git diff --check` passed.
- Independent read-only review checked scratch lifetime, early exits, per-worker separation, reference capture, and high/low valence capacity reuse. It did not run tests.

This is a performance change, so the functional probe is expected to pass on both revisions. No failing functional regression is claimed. No unrelated full suite, platform, or sanitizer validation is claimed.

## Reproduction

Build each revision in a clean directory with the same CMake options listed in `round2-manifold-tangent-scratch-metadata.json`. `MANIFOLD_PAR=OFF` selects serial. For parallel, use `MANIFOLD_PAR=ON` and `MANIFOLD_USE_BUILTIN_TBB=ON`; the repository fetches the pinned oneTBB version. The build logs/configure logs retain the exact environment used here.

Compile the probe against the revision under test:

```sh
c++ -std=c++17 -O3 -I include -I build/include round2_manifold_tangent_scratch_bench.cpp -L build/src -lmanifold -o probe
mkdir output
LD_LIBRARY_PATH="$PWD/build/src" taskset -c 0 ./probe output > result.csv
```

Use `taskset -c 0,1` for the parallel build. Keep a separate output directory per revision and compare every `.bin` file with `cmp`. Ensure `LD_LIBRARY_PATH` names the intended library; the metadata includes all four actual library SHA-256 hashes. On a host with different available CPU IDs, choose one/two available CPUs and record them.
