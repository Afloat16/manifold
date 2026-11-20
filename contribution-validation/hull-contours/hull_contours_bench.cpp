// Linux/glibc public-API allocation probe. The library is not instrumented.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "manifold/cross_section.h"

static std::atomic<bool> record{false};
static std::atomic<unsigned long long> allocated{0}, allocations{0};
extern "C" void* __libc_malloc(size_t) noexcept;
extern "C" void* malloc(size_t size) noexcept {
  void* result = __libc_malloc(size);
  if (record.load(std::memory_order_relaxed)) {
    allocated.fetch_add(size, std::memory_order_relaxed);
    allocations.fetch_add(1, std::memory_order_relaxed);
  }
  return result;
}
using namespace manifold;
static uint64_t Hash(const Polygons& paths) {
  uint64_t hash = 1469598103934665603ull;
  for (const auto& path : paths) {
    hash = (hash ^ path.size()) * 1099511628211ull;
    const auto* bytes = reinterpret_cast<const unsigned char*>(path.data());
    for (size_t i = 0; i < path.size() * sizeof(vec2); ++i)
      hash = (hash ^ bytes[i]) * 1099511628211ull;
  }
  return hash;
}
static Polygons Input(int count, int contours) {
  Polygons paths(contours);
  for (int path = 0; path < contours; ++path) {
    const int size = count / contours + (path < count % contours ? 1 : 0);
    auto& points = paths[path];
    points.reserve(size);
    for (int i = 0; i < size; ++i) {
      const double angle = 2.0 * 3.14159265358979323846 * i / size;
      points.push_back({3.0 * (path % 8) + std::cos(angle),
                        3.0 * (path / 8) + std::sin(angle)});
    }
  }
  return paths;
}
int main() {
  std::puts("points,contours,output_vertices,median_us,iqr_us,malloc_bytes,malloc_calls,output_hash,input_unchanged,area");
  for (int count : {8192, 100000, 1000000}) {
    for (int contours : {1, 64}) {
      const Polygons input = Input(count, contours);
      const auto before = Hash(input);
      CrossSection::Hull(input).NumVert();
      std::vector<double> timings;
      uint64_t hash = 0;
      size_t vertices = 0;
      double area = 0;
      unsigned long long bytes = 0, calls = 0;
      for (int run = 0; run < 5; ++run) {
        allocated = 0;
        allocations = 0;
        const auto start = std::chrono::steady_clock::now();
        record = true;
        CrossSection result = CrossSection::Hull(input);
        record = false;
        const auto end = std::chrono::steady_clock::now();
        bytes = allocated;
        calls = allocations;
        timings.push_back(std::chrono::duration<double, std::micro>(end - start).count());
        const auto currentHash = Hash(result.ToPolygons());
        if (run && currentHash != hash) return 2;
        hash = currentHash;
        vertices = result.NumVert();
        area = result.Area();
      }
      std::sort(timings.begin(), timings.end());
      std::printf("%d,%d,%zu,%.3f,%.3f,%llu,%llu,%016llx,%d,%.17g\n",
                  count, contours, vertices, timings[2], timings[3] - timings[1],
                  bytes, calls, static_cast<unsigned long long>(hash),
                  before == Hash(input), area);
    }
  }
}
