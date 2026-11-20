# Finite samples for valid coarse LevelSet grids

Source base: `6ca62006abf4579f60c40ee7b6c49b66cc4ae8fc`.
Published source: `9a1c480dcdce191d30be171d31f4e87103802051`, tree `5f9828619465161576b284454f25e6cc96c2b340`.

A legal positive edge length greater than a positive box extent formerly truncated that axis to one grid point, then divided its span by zero. A zero-length extent divided zero by zero. Both paths supplied non-finite coordinates to the user callback. Positive axes now keep at least their two endpoints. Zero extents keep one point with zero spacing and retain their previous empty result.

The public probe runs a constant-positive callback through `Manifold::LevelSet`; it explicitly records call count, non-finite coordinates, status, triangles and volume. `before.csv` is from the unchanged master library (archive identity and SHA-256 are in metadata), and `after.csv` is from the new library. The normal 2x2x2 box at edge length0.5 retains all six recorded values. Coarse edge3 changes an empty result into volume8 with finite samples. Three thin-axis variants change empty results into volume0.4. The zero-span case retains NoError/empty while eliminating non-finite callback samples. The constant field is allowed by the documented callback contract; no true signed-distance assumption is introduced.

## Reproduction

Configure a strict Release Ninja build with MANIFOLD_TEST, MANIFOLD_DEBUG, MANIFOLD_ASSERT, MANIFOLD_STRICT, MANIFOLD_CBIND and MANIFOLD_PYBIND enabled and MANIFOLD_PAR disabled. Build with one job. The cached googletest source path and toolchain configuration are in configure.log; 98/98 steps are in build.log.

Run the built manifold_test binary with `--gtest_filter=SDF.*:Context.LevelSet*:CBIND.*`: 45 pass across three suites. New regressions plus three existing controls are separately recorded in targeted.log. The same test binary loaded the archived original library through LD_LIBRARY_PATH for baseline-tests.log: four behavior failures and three controls passing. No tests or assertions were weakened for this comparison.

Compile `round2_manifold_levelset_coarse_probe.cpp` against the public headers/library and run it once with each library selected by LD_LIBRARY_PATH. The archived original library SHA-256 matches the prior unchanged-source identity. No benchmark speedup is claimed.

All three changed source files pass clang-format20.1.8 dry-run with Werror, and git diff --check passes. Independent review was read-only and found no blocker; it did not execute tests. TBB, sanitizer, other native suites, other compilers and WASM are unverified locally. Existing invalid-argument, oversize, progress and cancellation paths remain in place.
