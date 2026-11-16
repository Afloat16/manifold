# Requested curvature-channel validation

Candidate: `395023a8a3f6442cf87bb2dceea4fc8e61f9b266`, tree `5831c6a6acc9ed825789bb1388d06dcc9c70a578` (identical to local `f8e773ca325273faf985c5516672bcad144af7ab`).
Baseline: `74493803a4d8469779cdf9d8e3e23a11b349c22f`.

GCC 14.2; CMake Release; MANIFOLD_PAR=OFF; MANIFOLD_DEBUG=ON; MANIFOLD_ASSERT=ON; MANIFOLD_STRICT=ON. STRICT enables warnings as errors. No production source is instrumented.

The Linux/glibc probe counts malloc/calloc/realloc requests during public CalculateCurvature. Each sample also saves all 11 public MeshGL64 fields. It records median calling-thread CPU time over seven calls. Two orders were used: before/after, then after/before. The timings vary, including controls, and do not support a general speedup claim. Stable allocation savings are one 8-byte-per-geometric-vertex buffer for each single-channel request. Both/aliased/neither/empty controls retain their allocation counts and requested bytes.

Example for both separately built shared libraries:

```sh
cmake -S CHECKOUT -B BUILD -DCMAKE_BUILD_TYPE=Release -DMANIFOLD_PAR=OFF -DMANIFOLD_DEBUG=ON -DMANIFOLD_ASSERT=ON -DMANIFOLD_STRICT=ON
cmake --build BUILD --target manifold manifold_test -j1
g++ -std=c++17 -O2 -pthread -ICHECKOUT/include -IBUILD/include benchmark.cpp -LBUILD/src -Wl,-rpath,ABSOLUTE_BUILD/src -lmanifold -o benchmark
./benchmark OUTPUT_DIRECTORY >results.csv
cd CHECKOUT/test
ABSOLUTE_BUILD/test/manifold_test --gtest_filter='Properties.*:Smooth.*:Manifold.ErrorPropagationCalculateCurvature'
```

The 36 selected native cases pass. Twelve benchmark results and four input snapshots match byte for byte (37,635,779 serialized bytes), and each input's before/after snapshots agree. Original mesh IDs are deterministic only between fresh processes executing the same operations, so compare separate benchmark runs.

Historical audit covers all upstream PR states and old fork intermediate commits: 151 property implementation blobs, 1,416 implementation/export blobs across old paths, 47 curvature-related function variants (including 17 CurvatureAngles variants), and 10 older GetCurvature variants. Older GetCurvature always returned both results; no equivalent selective-channel implementation was found. Open PR1620 addresses degenerate faces and zero-area normalization and is a different change.
