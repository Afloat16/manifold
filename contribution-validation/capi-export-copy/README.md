# C API MeshGL export allocation check

The four C API exports construct a heap-owned mesh from the value returned by the C++ API. The patch removes the redundant lvalue copy of all buffers.

Base: 74493803a4d8469779cdf9d8e3e23a11b349c22f
Patch: c09c5b3b28a7e880f17e2ca644d78a81972f493d

The accompanying C++ benchmark calls the actual C API, compares every MeshGL field with the C++ value export, checks all four returned meshes after deleting the source, and records requested C++ allocation bytes for one export. These counters are not process RSS. Run with MANIFOLD_PAR=OFF.

For each revision, configure and build the shared C and C++ libraries, then compile the benchmark against that build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMANIFOLD_PAR=OFF -DMANIFOLD_TEST=OFF -DMANIFOLD_PYBIND=OFF -DMANIFOLD_CBIND=ON -DMANIFOLD_STRICT=ON -DMANIFOLD_DEBUG=ON -DMANIFOLD_ASSERT=ON
cmake --build build --target manifoldc -j2
g++ -std=c++17 -O3 -Iinclude -Ibindings/c/include -Ibindings/c contribution-validation/capi-export-copy-benchmark.cpp -Lbuild/src -Lbuild/bindings/c -Wl,-rpath,"$PWD/build/src" -Wl,-rpath,"$PWD/build/bindings/c" -lmanifoldc -lmanifold -o capi-export-copy-benchmark
./capi-export-copy-benchmark 1024
```

524,288 triangles, 262,146 vertices, six position/normal properties:
- 32-bit export allocated bytes: 36,700,758 -> 24,117,785 (12,582,973 fewer).
- 64-bit export allocated bytes: 61,866,698 -> 36,700,757 (25,165,941 fewer).
- Explicit-normal variants have identical allocation savings.
- The saved bytes equal one complete mesh buffer payload; returned live ownership is unchanged.
- Repeat runs at 32,768 triangles confirm the same relationship.

Existing native CBIND tests: 25 passed, with strict compilation, debug tracing and assertions enabled. Detailed before/after data are in results.json and native-tests.log.
