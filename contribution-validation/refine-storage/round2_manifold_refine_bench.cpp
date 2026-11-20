// Linux/glibc-only public API allocation/timing probe. No library instrumentation.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include "manifold/manifold.h"

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
using manifold::Manifold;
using manifold::MeshGL64;
static uint64_t Hash(const MeshGL64& mesh) {
  uint64_t hash = 1469598103934665603ull;
  auto mix = [&](const auto& array) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(array.data());
    for (size_t i = 0; i < array.size() * sizeof(array[0]); ++i)
      hash = (hash ^ bytes[i]) * 1099511628211ull;
  };
  mix(mesh.vertProperties); mix(mesh.triVerts); mix(mesh.halfedgeTangent);
  mix(mesh.mergeFromVert); mix(mesh.mergeToVert); mix(mesh.runIndex); mix(mesh.faceID);
  return hash;
}
static Manifold Refine(const Manifold& input, int mode, int segments) {
  if (mode == 0) return input.Refine(2);
  if (mode == 1) return input.RefineToLength(2.0 / segments);
  return input.RefineToTolerance(0.25 / (segments * segments));
}
int main() {
  std::puts("case,segments,input_triangles,output_triangles,median_us,iqr_us,malloc_bytes,malloc_calls,output_hash,input_unchanged,status");
  for (int segments : {16, 64, 128}) {
    for (int mode = 0; mode < 3; ++mode) {
      Manifold input = Manifold::Sphere(1, segments);
      if (mode == 2) input = input.CalculateNormals(0, 180).SmoothByNormals(0);
      const auto before = Hash(input.GetMeshGL64());
      const Manifold alias = input;
      std::vector<double> timings;
      Refine(input, mode, segments).NumTri();
      uint64_t hash = 0;
      int tris = 0, status = -1;
      unsigned long long bytes = 0, calls = 0;
      for (int run = 0; run < 5; ++run) {
        allocated = 0; allocations = 0;
        const auto start = std::chrono::steady_clock::now();
        record = true;
        Manifold result = Refine(input, mode, segments);
        tris = result.NumTri();
        record = false;
        const auto end = std::chrono::steady_clock::now();
        bytes = allocated; calls = allocations;
        timings.push_back(std::chrono::duration<double, std::micro>(end - start).count());
        const auto currentHash = Hash(result.GetMeshGL64());
        if (run != 0 && currentHash != hash) return 2;
        hash = currentHash;
        status = static_cast<int>(result.Status());
      }
      std::sort(timings.begin(), timings.end());
      std::printf("%s,%d,%d,%d,%.3f,%.3f,%llu,%llu,%016llx,%d,%d\n",
        mode == 0 ? "refine" : mode == 1 ? "to_length" : "to_tolerance",
        segments, input.NumTri(), tris, timings[2], timings[3] - timings[1],
        bytes, calls, static_cast<unsigned long long>(hash),
        before == Hash(alias.GetMeshGL64()), status);
    }
  }
}

