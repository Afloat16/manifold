#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <vector>

#include "manifold/polygon.h"
#include "tree2d.h"
#include "vec.h"

using namespace manifold;

uint64_t Digest(const std::vector<ivec3>& triangles) {
  uint64_t hash = 14695981039346656037ull;
  for (const ivec3 triangle : triangles) {
    for (int i = 0; i < 3; ++i) {
      hash ^= static_cast<uint32_t>(triangle[i]);
      hash *= 1099511628211ull;
    }
  }
  return hash;
}

int main() {
  for (const int kind : {0, 1, 2}) {
    for (const int count : {128, 1024, 4096}) {
      Polygons polys(1);
      for (int i = 0; i < count; ++i) {
        const double angle = 2 * kPi * i / count;
        const double radius = kind == 0 ? (i % 2 ? 1.0 : 2.0)
                              : kind == 1 ? 2.0 + 0.15 * std::sin(19 * angle)
                                          : 2.0;
        polys[0].push_back({radius * std::cos(angle), radius * std::sin(angle)});
      }
      if (kind == 2) {
        polys.emplace_back();
        for (int i = 0; i < count / 2; ++i) {
          const double angle = -4 * kPi * i / count;
          polys[1].push_back({std::cos(angle), std::sin(angle)});
        }
      }
      const int repeats = count <= 128 ? 500 : count <= 1024 ? 40 : 5;
      double elapsed = 0;
      uint64_t digest = 0;
      size_t numTri = 0;
      for (int rep = 0; rep < repeats; ++rep) {
        const std::clock_t start = std::clock();
        const auto triangles = Triangulate(polys);
        elapsed += double(std::clock() - start) / CLOCKS_PER_SEC * 1000;
        digest = Digest(triangles);
        numTri = triangles.size();
      }
      std::printf("polygon%d %d %.9f %zu %llu\n", kind, count,
                  elapsed / repeats, numTri,
                  static_cast<unsigned long long>(digest));
    }
  }
}
