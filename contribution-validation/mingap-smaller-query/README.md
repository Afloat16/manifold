# Smaller-query MinGap validation

The public `Manifold::MinGap` previously always used `other` as the query mesh. Its helper computes a face box and Morton code for every query face, expands each box, and queries the first mesh's existing collider. With unequal mesh sizes, that can allocate and traverse many more query boxes than necessary. The helper's collision scan is explicitly sequential in the existing implementation.

The patch retains the current bounding-box rejection and solid-intersection check, then uses the mesh with fewer triangles as the queries and the other as the collider. Equal-size meshes retain their existing order. No triangle-distance arithmetic, search-cap formula or solid-containment handling is changed. Swapping arguments can change floating-point operation order; general bitwise equality for every possible mesh is not claimed.

## Source identity

- Upstream base, verified current before publication: `6ca62006abf4579f60c40ee7b6c49b66cc4ae8fc`.
- Public patch: `ac41dab6060eae5d36cb8e3c010d4a931db4eb61`.
- Equivalent local commit: `bd1f50cf01dbb0e6fdacde04401e3ea3c6f5a439`.
- Patch tree: `9f5d09427eb22809577554d38d0285c57982e9ef`.
- Both actual shared-library SHA-256 hashes are in the metadata JSON.

The base library is the preserved, pristine serial build of the same upstream base. The benchmark executable dynamically links the selected library through `LD_LIBRARY_PATH`; its public headers and ABI are the same for both. No geometry substitution or internal API is used.

## Measurement method

The included Linux/glibc C++ probe calls public `MinGap`, with the meshes created and evaluated before timing. Every case has one warmup and eleven measured calls, pinned to CPU 0. The CSV reports median/IQR and requested malloc bytes/calls for the measured operation. These are cumulative allocation traffic, not peak live memory or RSS. The probe compares the input MeshGL64 payloads before and after every case (`numProp` and all ten vectors), and checks each scalar result against an independent analytic gap/cap.

All 24 base/patch scalar outputs match exactly for these fixtures. Both inputs remain byte-for-byte unchanged. The harness covers both call directions for different-size meshes, an equal-size control, a wider search, bounding-box rejection, overlap, containment, empty geometry, zero limit and the existing negative-limit behavior.

Selected forward-direction results:

| Pair / search limit | Base malloc bytes | Patch malloc bytes | Median microseconds, base → patch | IQR microseconds, base → patch |
| --- | ---: | ---: | ---: | ---: |
| 12-face cube / 2,048-face sphere, 2.01 | 120,908 | 15,036 | 116.025 → 26.119 | 6.311 → 0.761 |
| 12-face cube / 32,768-face sphere, 2.01 | 1,902,668 | 199,356 | 1,643.820 → 212.010 | 139.801 → 18.728 |
| Same 32,768-face pair, 3.0 | 1,902,668 | 199,356 | 19,683.692 → 15,388.782 | 1,175.775 → 1,445.720 |
| 128-face sphere / 8,192-face sphere, 2.01 | 477,956 | 58,628 | 363.058 → 22.534 | 183.987 → 0.100 |
| Equal 12-face cubes, 2.01 | 2,820 | 2,820 | 7.582 → 7.571 | 0.140 → 0.149 |
| Equal 2,048-face spheres, 2.01 | 133,124 | 133,124 | 75.413 → 76.695 | 0.571 → 12.790 |

There are still 27 allocation calls in each of these cases; the two query arrays are smaller, not removed. The largest pair saves exactly `(32768 - 12) * (sizeof(Box) + sizeof(uint32_t)) = 1,703,312` requested bytes. Opposite-direction calls that already used the smaller mesh keep the same allocation totals. The early-rejected/empty cases allocate zero in both builds; overlap/containment keep 164,764 bytes. All timing samples, including slower controls, are retained. This shared CPU measurement does not promise a universal speedup, and wide searches still evaluate many of the same triangle pairs.

## Validation

- GCC 14.2 Release, strict warnings/Werror, assertions/debug checks enabled, parallelism disabled, C/Python bindings enabled: **98/98 clean build steps passed**.
- The two added analytic-gap/retessellation tests also pass with the original library: these are behavioral controls for a performance change, not failing correctness regressions.
- Complete `Measurement.*` suite with the patch: **41 passed in 9.199 seconds**. This includes both new cases and the existing triangle-distance, ray, winding, capped-gap and solid-containment checks.
- `clang-format 20.1.8 --dry-run --Werror` on both changed files and `git diff --check` passed.
- Independent read-only review found no blocking issue and did not run tests.

Other native suites, TBB, other compilers, sanitizers and WASM are left to CI. The serial collision scan is unchanged; this is not a parallelization proposal.

## Reproduce

Build each source revision with the same CMake options and dependencies. Then compile the probe using its public include directories:

```sh
c++ -std=c++17 -O3 -I include -I build/include round2_manifold_mingap_query_bench.cpp -L build/src -lmanifold -o probe
LD_LIBRARY_PATH="$PWD/build/src" taskset -c 0 ./probe > result.csv
build/test/manifold_test --gtest_filter='Measurement.*'
```

Use an allowed CPU if the host does not expose CPU 0, and record it. The benchmark is a Linux/glibc allocation probe; the production change is portable C++.

## History boundary

All-author MinGap/smaller/operand searches and 79 contributor fork branches were checked. #1997 adds the bounding-box rejection already retained here. #2026 repairs nearly parallel triangle-edge arithmetic; that algorithm is unchanged. The older contributor branch `perf/reuse-gap-query-bounds-20261116` (`55819ed...`) reuses cached bounds and avoids unused Morton calculations inside the helper. This patch makes the independent outer choice of which mesh supplies the queries and composes with that helper optimization.
