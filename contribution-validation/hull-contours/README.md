# Raw-contour convex hull allocation comparison

This compares public `CrossSection::Hull(const Polygons&)` on upstream master
`7fae9769195d1d4eccf373cbc0491f754eb6c193` and the candidate source tree
`2f26017cc1329e88b2e361971e1713dff4257d89`.

The input is generated before measurement. One contour is a densely sampled unit
circle; 64 contours are unit circles on an 8 by 8 grid. Total input sizes are
8,192, 100,000 and 1,000,000 points. The library is built in Release with GCC 14.2,
strict/debug/assert checks enabled, and the parallel backend disabled. The public
API harness is compiled once and dynamically linked against each library.

The Linux/glibc-only malloc interposer counts the sizes requested during the
public call. The reported bytes and call count are from the final measured call;
they are cumulative allocation traffic, not peak live storage, RSS, retained
memory, or frees. The harness does not change the library source. Each case gets
one warmup and five measured calls. Median and IQR timing are included, but this
shared runner is noisy and no dependable latency improvement is claimed.

All six base/branch output hashes, vertex counts and areas match exactly. Every
case leaves the original contours unchanged. At one million input points the
candidate removes 33,554,416 requested bytes and 21 malloc calls, for both one
contour and 64 contours. The 64-contour call changes from 58,328,784 to 24,774,368
requested bytes. Degenerate/invalid input and const-input behavior also have
native regression coverage; the targeted native run passed 193 tests across
CrossSection, CrossSectionComplex, CrossSectionOffsetCorpus and CBIND.

Reproduce with the accompanying C++ source, using the same compiler/options for
both revisions, substituting their built library directories:

```sh
c++ -std=c++17 -O2 -I <checkout>/include hull_contours_bench.cpp \
  -L <base-build>/src -lmanifold -o hull_contours_bench
LD_LIBRARY_PATH=<base-build>/src ./hull_contours_bench > base.csv
LD_LIBRARY_PATH=<branch-build>/src ./hull_contours_bench > branch.csv
```

The separate output hash and input hash checks run after recording is disabled.
The comparison preserves input point order before `HullImpl` sorts its local
buffer. Sorting, orientation and non-finite/degenerate handling are unchanged.
