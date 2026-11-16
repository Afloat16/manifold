Level-set sampling currently copies the SDF target on every bounded query and surface-intersection call, including calls that return a boundary value, accept an endpoint, or interpolate without evaluating the callback. A heap-allocated callback target therefore causes allocations even when no SDF work is needed.

This change moves each helper's private callback copy past those early exits. An actual bounded evaluation still owns its own target, and each independent root search still owns one target for all of that search's iterations. Mutable state stored by value in a callback remains isolated between evaluations/searches.

## Validation

- All **16** native SDF and registered LevelSet context tests pass with GCC 14.2, `MANIFOLD_PAR=OFF`, and strict/debug/assert checks enabled.
- A copy-count regression fails against the unchanged base and passes with the change, covering interpolation and refined surface searches.
- A mutable callback regression checks a non-atomic per-target counter and a callback whose return value changes after its first invocation. It checks sequential and parallel calls, preserves one evaluation per target during interpolation, and preserves state across iterations of one root search.
- The unchanged base plus the mutable regression passes **656/656** tests under the existing parallel ThreadSanitizer workflow. The exact candidate passes **657/657**, including the copy-count regression. Both runs use `MANIFOLD_PAR=ON`, sanitized TBB, and `halt_on_error=1`; neither log contains a ThreadSanitizer warning. These are fork validation runs, not upstream PR checks.
- Eight public-API cases produce byte-identical complete `MeshGL64` payloads (**1,944,200 bytes**), including stateful interpolation/refinement. Callback evaluation counts are unchanged.
- In a fine sphere case with a heap-backed matrix capture, callback copies fall from **429,017 to 228,258**, allocation calls from **429,087 to 228,328**, and requested allocation bytes from **92,878,552 to 63,969,256**. These are allocation counts/bytes, not peak-memory or runtime claims.
- Coarser surfaces, refined searches, a clipped plane, and an empty result are also covered. Clang-format 20 and `git diff --check` pass.

Parallel validation: [baseline](https://github.com/Afloat16/manifold/actions/runs/43949632683), [candidate](https://github.com/Afloat16/manifold/actions/runs/43949770241).
