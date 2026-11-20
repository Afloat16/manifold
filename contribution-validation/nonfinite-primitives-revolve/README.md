# Revolve coordinate validation follow-up for PR #2030

This is the same non-finite primitive-input repair as PR #2030, not a separate contribution. `Revolve` already checks the angle but previously allowed NaN or infinite profile vertices into clipping, automatic segment selection and triangulation. `Extrude` rejects these vertices. A fully clipped invalid profile could also be silently ignored beside a valid profile.

The follow-up validates each contour with at least three vertices before testing or clipping its x coordinates. Contours with fewer than three vertices retain their existing ignored behavior. Negative angles, full revolutions and finite geometry are unchanged.

## Source identities

- Upstream base: `6ca62006abf4579f60c40ee7b6c49b66cc4ae8fc`.
- Original PR head: `1aaee04c6633f7e2495dc0c5bc25693685c776d6`.
- Follow-up public head: `2cd820f6fa6ffa48350290bb5684c23a449c9533`.
- Equivalent local follow-up commit: `edd6431ed06ad195e105d44efec93fa22ca3b2db`.
- Follow-up tree: `36cfb365dd32779ae38f54c40346b34964905a7c`.

Only `src/constructors.cpp` and `test/manifold_test.cpp` change in this follow-up. The public branch was advanced with an expected-head lease and no force update.

## Actual baseline evidence

The public C++ probe sets either coordinate of one vertex in `{{1,0},{2,0},{2,1},{1,1}}` to NaN or either infinity. Before the fix all six `Revolve` cases return `NoError`; some have NaN volume, others silently become empty or finite geometry. The same inputs to `Extrude` return `InvalidConstruction`. Its original library has the same Revolve/Extrude implementations as the upstream base and original PR head. The CSV is retained rather than inferred from code.

Separately, the two new native tests were built against the original PR implementation before adding the guard. One regression test failed, including a strict-debug triangulation exception for NaN, while the short-contour compatibility test passed. The complete raw failure output is retained. Since that exception stops a test body, this is one failing test, not a claim that every loop parameter separately ran and failed.

## Patched checks

- Strict GCC 14.2 Release build, assertions/debug checks on, C/Python bindings on, parallelism off: 17 incremental build steps passed after the production edit. The earlier original PR's clean 98-step build is recorded separately.
- Both new native tests passed. They cover NaN and both infinities, x/y, automatic and explicit segment counts, positive/negative half revolutions, full revolutions, a clipped invalid sibling contour, and preservation of ignored short contours.
- `Manifold.*:Properties.*:Smooth.*:CrossSection.*:CrossSectionComplex.*:CrossSectionOffsetCorpus.*:CBIND.*`: **362 passed across seven suites in 55.734 seconds**, including both new tests and all seven tests from the original PR.
- The public C++ probe with the patched library now returns empty `InvalidConstruction` for all six invalid profile cases, matching `Extrude`.
- `clang-format 20.1.8 --dry-run --Werror` on both changed files and `git diff --check` passed.
- Independent read-only review checked validation before clipping and preservation of ignored short contours; it found no blocker and did not run tests.

These are selected affected native suites. Other native suites, TBB, other compilers, sanitizer and WASM configurations are left to CI. The change is restricted to non-finite input coordinates; it makes no promise that arbitrary extreme finite coordinates cannot overflow later arithmetic.

## Commands

From the configured source checkout:

```sh
cmake --build build-finite-primitives --parallel 1
build-finite-primitives/test/manifold_test --gtest_filter='Manifold.NonFiniteRevolveCoordinatesAreInvalid:Manifold.RevolveStillIgnoresShortContours'
build-finite-primitives/test/manifold_test --gtest_filter='Manifold.*:Properties.*:Smooth.*:CrossSection.*:CrossSectionComplex.*:CrossSectionOffsetCorpus.*:CBIND.*'
clang-format --dry-run --Werror src/constructors.cpp test/manifold_test.cpp
git diff --check
```

Compile the included public probe against the project's public headers and shared `libmanifold`, then select the corresponding baseline/patched library through `LD_LIBRARY_PATH`. The probe does not access internal APIs or replace geometry calculations. All logs and both CSVs are included.
