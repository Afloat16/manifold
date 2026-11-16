# MeshGL property-bin allocation validation

Source base: `74493803a4d8469779cdf9d8e3e23a11b349c22f`.
Candidate: `2009ea8c75d844569faeb07caacddc5586b90f89`.
GCC 14.2, C++17, Release, MANIFOLD_PAR=OFF, MANIFOLD_STRICT=ON, MANIFOLD_DEBUG=ON, MANIFOLD_ASSERT=ON.

The native C++ benchmark overrides global new/delete and records requested allocation bytes, peak live requested bytes, allocation count, and full-field binary exports. These are allocation counters rather than process RSS. Run the same executable against the unchanged and patched shared libraries with LD_LIBRARY_PATH. The first argument is the output directory for binary snapshots; the optional second argument is the sphere's circular-segment count (1024 by default). The 40 before/after snapshots match byte-for-byte; SHA256 values and byte counts are in equivalence.json.

All 644 registered native tests passed. The first session was externally interrupted after 331 successes; the other 313 completed on the identical binary and commit. No tests were excluded. Both logs and the unique complete test-name list are preserved.

Wall-clock measurements were collected on a shared host and are noisy (including on the unchanged no-properties control), so this change makes no latency claim. The deterministic allocation savings are the performance result.
