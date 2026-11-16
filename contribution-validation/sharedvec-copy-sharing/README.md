# SharedVec copy-construction validation

Candidate: b6ab77889b996ed19348e58ea6c26684f58bf740. Base: 74493803a4d8469779cdf9d8e3e23a11b349c22f.

The production branch changes only src/vec.h and its native regression tests. These separate validation files preserve the allocation measurement and complete test output.

The benchmark interposes glibc malloc/calloc/realloc while the public operation runs and writes every public MeshGL64 field to binary output. Compare both exported bytes and allocation counters; counters are requested allocations, not RSS. Run each binary against its corresponding shared Manifold library; the two required arguments are the output directory and sphere segment count. The recorded measurements use 512 segments. No latency improvement is claimed.

649 local strict native tests pass. The exact remote candidate also passes 660 tests under the repository's existing GCC 14/TBB ThreadSanitizer workflow: https://github.com/Afloat16/manifold/actions/runs/43936963443 . The TSAN build registers 15 additional parallel-build cases. Unlike the local build, it does not enable MANIFOLD_DEBUG, so its suite does not register the four guarded BooleanComplex cases (InfiniteRecursion, OffsetSelfIntersect, OffsetTriangulationFailure and OpenscadCrash). Those four cases passed in the local native suite. Both new concurrent tests executed in TSAN.

The header ownership tests were compiled with ASan/UBSan. ASAN_OPTIONS=detect_leaks=0 is necessary in the traced local runtime, so these logs do not establish LeakSanitizer coverage.
