# Safe LevelSet callback validation

Candidate: 784c68b3889659e746d5ecad4514ea2771c8e4d9. This branch preserves measured results and full completed parallel TSAN logs after the local execution environment became unavailable. The candidate remains unpublished upstream. Its product source and native regressions are in the parent commit.

The earlier shared-reference candidate was rejected because it changed mutable callback ownership. Do not publish that candidate or reuse its 429,017 → 17 copy count. The safe lazy-copy version was actually remeasured: 429,017 → 228,258 copies for the fine case, with byte-identical full mesh outputs and unchanged evaluation counts.

The complete TSAN logs cover the unchanged baseline plus a mutable-state test and the exact safe candidate. The local benchmark source, serialized payload files, and native logs were saved under audit/manifold-sdf-v2-* and audit/manifold_sdf_callback_benchmark_v2.cpp before executor loss; these local files are not represented here as uploaded raw artifacts. Results and payload hashes are preserved in results.json.

The separate Minkowski directory records the already-pushed #1971 review follow-up, its verified current body, and the reply draft that could not be posted because the GitHub integration returned 403. No additional upstream contribution is counted for that follow-up.
