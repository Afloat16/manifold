# Query normal storage validation

The source change removes unused normal buffers from the temporary query-side implementations used by `Manifold::WindingNumber` and `Manifold::RayCast`. Both use `Moved::Still`; its zero scale returns the query position before reading any normal. The queried solid still uses its original vertex/face normals, including reported `RayHit.normal` values. Query geometry, tie rules, BVH traversal and empty/degenerate early returns are unchanged.

- Upstream base: `6ca62006abf4579f60c40ee7b6c49b66cc4ae8fc`.
- Published patch: `ddc9ec5a2f1aed841fb5c45b1257e3757991c6dc`.
- Local commit: `506cc924b91166f53537a6b053eb5f4fd74dd815`; its tree matches the published patch (`744d5267a30284874a7b8bc20ec89fb95bfc9ca1`).
- GCC 14.2, strict Release, assertions/debug checks enabled, serial execution, C/Python bindings enabled: 98/98 build steps passed.
- Complete `Measurement.*` selection: 39 passed in 9.438 seconds.
- clang-format 20.1.8 and whitespace checks passed.
- Independent read-only review traced all query-side displacement reads and the temporary Impl lifetime, with no blocker. The reviewer did not run tests.

## Public API comparison

The executable is the same for both runs; `LD_LIBRARY_PATH` selects the preserved pristine upstream library or the patch library. See metadata for exact library SHA-256 values. Every case runs one warmup and 11 measured calls. Timings are medians and interquartile ranges on a shared, unpinned CPU. Byte counts are cumulative requested bytes observed by a Linux/glibc `malloc` interposer during the public API call, not peak live memory, RSS, or total system allocation. This is a reduction in unnecessary allocation, not a general speedup claim.

| Workload | Requested malloc bytes, base -> patch | Malloc calls | Median microseconds (IQR), base -> patch |
| --- | ---: | ---: | ---: |
| Winding: 131072 inside cube | 7864332 -> 4718604 | 8 -> 7 | 68896.388 (13888.600) -> 66439.311 (6479.806) |
| Winding: 131072 alternating inside/outside | 4718604 -> 3145740 | 8 -> 7 | 31606.491 (1029.332) -> 33277.073 (2360.932) |
| Winding: 131072 outside cube | 1572864 -> 1572864 | 3 -> 3 | 245.541 (21.652) -> 239.221 (16.274) |
| Ray through cube | 384 -> 312 | 11 -> 9 | 2.294 (0.241) -> 2.244 (0.209) |
| Ray from cube interior | 240 -> 168 | 10 -> 8 | 1.252 (0.080) -> 1.202 (0.080) |
| Ray missing cube | 168 -> 96 | 9 -> 7 | 0.190 (0.031) -> 0.160 (0.021) |
| Zero-length ray | 0 -> 0 | 0 -> 0 | 0.050 (0.001) -> 0.050 (0.010) |

The winding allocation reduction is exactly 24 bytes per retained point; points excluded by the existing bounding-box filter require no normal buffer before or after. Nonempty, nondegenerate ray queries remove two normal-buffer allocations totaling 72 requested bytes. Slower timing rows remain in the CSV, including mixed winding and shared-vertex rays; CPU timing does not establish a universal improvement.

All 27 before/after workload names, mesh/point counts, output lengths and serialized result hashes match. The probe also checks analytic winding/crossing controls, finite ordered ray hits, unchanged input points and exact preservation of the source mesh's full MeshGL64 payload (all ten vector fields plus numProp). Workloads include zero/one/small/large point batches, inside/mixed/outside queries, hollow and dense solids, empty meshes, axis-aligned/missing/interior/shared-vertex rays and zero-length rays. Functional regressions use the existing complete Measurement suite; no new mirror test is added for these removed internal allocations.

## Reproduction

Build the base and patch with the settings in metadata/configure logs. Compile the public probe against the unchanged public ABI:

```sh
c++ -std=c++17 -O3 -I include -I build/include round2_manifold_query_normals_bench.cpp -L build/src -lmanifold -o probe
LD_LIBRARY_PATH=build/src ./probe > result.csv
build/test/manifold_test --gtest_filter='Measurement.*'
```

Run once with each library. `before.csv` and `after.csv` retain every timing and allocation row. This local validation is serial; TBB, other compilers, sanitizers and WASM remain for repository CI.
