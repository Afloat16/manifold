# LevelSet callback-copy validation

Candidate: `a2e56909af490fb54c7cf2e9c9e292ec14d53ac4` (same complete tree as local `ca5f9e7552052e464c26d5475c29b0e75a8ddab4`).
Baseline: `74493803a4d8469779cdf9d8e3e23a11b349c22f`.

GCC 14.2; CMake Release; MANIFOLD_PAR=OFF; MANIFOLD_DEBUG=ON; MANIFOLD_ASSERT=ON; MANIFOLD_STRICT=ON. STRICT enables compiler warnings as errors. No production source is instrumented.

`benchmark.cpp` is a Linux/glibc allocation probe. It counts requested malloc/calloc/realloc bytes during public LevelSet construction, using a nontrivial callback holding a transform. It saves every public MeshGL64 field after each operation. It does not measure peak memory or promise speedups. Run the same executable source against the two separately built shared libraries. Both runs must start in fresh processes so original mesh IDs match.

Example for each checkout/build:

```sh
cmake -S CHECKOUT -B BUILD -DCMAKE_BUILD_TYPE=Release -DMANIFOLD_PAR=OFF -DMANIFOLD_DEBUG=ON -DMANIFOLD_ASSERT=ON -DMANIFOLD_STRICT=ON
cmake --build BUILD --target manifold manifold_test -j1
g++ -std=c++17 -O2 -pthread -ICHECKOUT/include -IBUILD/include benchmark.cpp -LBUILD/src -Wl,-rpath,ABSOLUTE_BUILD/src -lmanifold -o benchmark
./benchmark OUTPUT_DIRECTORY >results.csv
cd CHECKOUT/test
ABSOLUTE_BUILD/test/manifold_test --gtest_filter='SDF.*:Context.LevelSet*'
```

The new CallbackCopies regression was also built as a standalone GoogleTest against the unchanged baseline library; both tolerance checks failed because copies scaled with samples/edges. It passes against the candidate.

All 12 SDF tests and the 3 registered LevelSet context tests passed. Context.LevelSetCancelConcurrent is compiled only for MANIFOLD_PAR=1 and was not registered in this serial build; it is not counted as passed. Upstream CI has a separate parallel/TSAN configuration.

Six complete exported meshes are byte-identical, totaling 1,563,438 serialized bytes; `results.json` records every payload hash and both allocation-count tables. Source audit includes all upstream PR states and old fork intermediate commits under seven historical SDF paths, with 168 unique source blobs and no equivalent helper-reference change.
