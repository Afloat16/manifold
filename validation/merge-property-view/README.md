# MeshGL Merge property view validation

Baseline: `08723c1b9f72225d1b63037f8a95e48387661b14`.
Candidate: replace the owning `Vec<Precision>` used by the bounding-box scan in
`src/sort.cpp` with `const VecView<const Precision>`.

Linux x86-64, GCC 14.2.0, Release; `MANIFOLD_STRICT=ON`,
`MANIFOLD_DEBUG=ON`, `MANIFOLD_ASSERT=ON`, `MANIFOLD_PAR=OFF`.
C and Python bindings built successfully. No source files other than `src/sort.cpp`
are part of the production patch.

`merge_probe.cpp` calls public `MeshGL::Merge` and `MeshGL64::Merge` on
triangle soups made from 1, 100, or 1,000 disjoint cubes, with 3, 9, or 64
property channels. Two already-closed meshes are controls. It verifies that
input payloads are unchanged and hashes all output fields, including triangle
indices, merge mappings, property values and metadata. Every one of the 20
baseline and candidate hashes matches.

The executable intercepts `malloc` around the public call, counting requested
bytes (not peak live memory or process RSS). All 18 open-mesh cases remove one
allocation whose size is exactly `NumVert() * numProp * sizeof(Precision)`;
the two closed controls are unchanged. A 36,000-vertex, 64-property input uses
28,460,216 -> 19,244,216 requested bytes for float and
47,560,496 -> 29,128,496 for double. The returned `MeshGL` still owns its copy.
Timing columns are retained as raw samples but shared-host load was noisy;
no throughput improvement is claimed from them.

Commands (adapt checkout/build paths):

```sh
c++ -std=c++17 -O3 -Iinclude -Ibuild/include merge_probe.cpp -Lbuild/src -lmanifold -Wl,--export-dynamic -o merge_probe
LD_LIBRARY_PATH=/path/to/base/build/src ./merge_probe > merge-base.csv
LD_LIBRARY_PATH=/path/to/candidate/build/src ./merge_probe > merge-branch.csv
build/test/manifold_test --gtest_filter='*Merge*'
```

All 15 Merge-related tests pass for the final candidate. The broader native run
passed 692 tests using the preliminary non-owning `const std::vector&` spelling;
its log is retained. The final `const VecView` spelling preserves the original
raw-pointer iterator type for the strided scan and was rechecked with the 15
Merge tests and all 20 public-API compatibility/allocation cases.

The remaining native death test, `Utils.VecAbortsWhenAllocationFails`, could not
run because GoogleTest tries to create its capture file under `/tmp`, which is
unavailable in the validation environment. No test skip/xfail markers were changed.
Clang-format 20.1.8 and `git diff --check` passed. Parallel/TBB, WASM, and other
platforms were not run locally.
