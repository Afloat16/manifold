# Triangulation query benchmark

Compares base `4a6e01bef00ad707dbc976b31e4728782e398f9d` with candidate `54ae12bd7b148849b035b6d76db136249560eaff`.

Build each revision in a separate directory with GCC 14.2, Release (`-O3`), `MANIFOLD_STRICT=ON`, `MANIFOLD_DEBUG=ON`, `MANIFOLD_ASSERT=ON`, `MANIFOLD_PAR=OFF`. The original measurements used CMake 4.4.5 and Linux, with one process pinned to available CPU 0. No whole-program or link-time optimization.

Compile `probe.cpp` against either matching source/build include directories and link `libmanifold`:

```sh
g++ -O3 -std=c++17 -I<SOURCE>/include -I<SOURCE>/src -I<BUILD>/include probe.cpp -L<BUILD>/src -lmanifold -o probe
LD_LIBRARY_PATH=<BUILD>/src ./probe
```

The probe reports complete public `Triangulate` CPU times in milliseconds, triangle counts, and FNV-1a hashes of every ordered triangle index. Run in alternating base/changed order five times on the same pinned CPU. `measurements.json` contains all samples and medians. Each run repeats the 128/1024/4096 outer-vertex cases 500/40/5 times. Shapes are an alternating-radius star, a smooth 19-lobed contour, and an annulus with half as many inner vertices. Inputs are synthetic; measured improvements do not promise speedups for other shapes or hardware.

All nine triangle counts and hashes matched across both revisions and all runs. The accompanying code change passed all 127 `Utils.*:Polygon.*` tests, including the newly added disabled-pivot regression (which fails on the base).
