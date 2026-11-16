Thank you for checking this and for the hull-equivalence observation. The follow-up commit `2da50113` keeps the original operand order for erosion and routes convex-base erosion through the existing per-A-face/all-B-vertices hull path: for convex A, `A ⊖ B = A ⊖ conv(B)`. The final branch is now a plain `else`.

I reproduced the enclosing-cross failure against the initial PR head: all four large-cutter assertions fail there. They now pass with centered/negatively translated bases and 0°/90° cutter rotations. I also added a rotated, translated tetrahedron check against erosion by `cutter.Hull()`, with both directional set differences empty. The 9 Minkowski tests and the original-ID control test all pass with assertions and strict/debug checks enabled (`MANIFOLD_PAR=OFF`).

The PR description now reflects this routing. This addresses the convex-base case here; the non-convex-base and empty-base cases in #1980 remain separate.
