#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>

#include "conv.h"
#include "manifold/manifoldc.h"

// Count requested C++ allocation bytes inside one public export call.
// This benchmark uses MANIFOLD_PAR=OFF, so these counters are single-threaded.
namespace allocation {
struct alignas(std::max_align_t) Header {
  size_t size;
  size_t generation;
};
bool active = false;
size_t generation = 0, total = 0, live = 0, peak = 0, count = 0;
void begin() {
  ++generation;
  total = live = peak = count = 0;
  active = true;
}
}

void* operator new(size_t size) {
  auto h = static_cast<allocation::Header*>(
      std::malloc(sizeof(allocation::Header) + std::max(size, size_t(1))));
  if (!h) throw std::bad_alloc();
  h->size = size;
  h->generation = allocation::active ? allocation::generation : 0;
  if (allocation::active) {
    allocation::total += size;
    allocation::live += size;
    allocation::peak = std::max(allocation::peak, allocation::live);
    ++allocation::count;
  }
  return h + 1;
}
void operator delete(void* p) noexcept {
  if (!p) return;
  auto h = static_cast<allocation::Header*>(p) - 1;
  if (allocation::active && h->generation == allocation::generation)
    allocation::live -= h->size;
  std::free(h);
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, size_t) noexcept { ::operator delete(p); }

template <typename Mesh>
void checkEqual(const Mesh& a, const Mesh& b) {
  if (a.numProp != b.numProp || a.vertProperties != b.vertProperties ||
      a.triVerts != b.triVerts || a.mergeFromVert != b.mergeFromVert ||
      a.mergeToVert != b.mergeToVert || a.runIndex != b.runIndex ||
      a.runOriginalID != b.runOriginalID || a.runTransform != b.runTransform ||
      a.faceID != b.faceID || a.halfedgeTangent != b.halfedgeTangent ||
      a.runFlags != b.runFlags)
    throw std::runtime_error("C export differs from the C++ value export");
}

template <typename T>
size_t bytes(const std::vector<T>& v) { return sizeof(T) * v.size(); }

template <typename Mesh>
size_t payloadBytes(const Mesh& mesh) {
  return bytes(mesh.vertProperties) + bytes(mesh.triVerts) +
         bytes(mesh.mergeFromVert) + bytes(mesh.mergeToVert) +
         bytes(mesh.runIndex) + bytes(mesh.runOriginalID) +
         bytes(mesh.runTransform) + bytes(mesh.faceID) +
         bytes(mesh.halfedgeTangent) + bytes(mesh.runFlags);
}

template <typename Export, typename Delete, typename Mesh>
void measure(const char* kind, int segments, ManifoldManifold* m,
             Export exportMesh, Delete deleteMesh, const Mesh& expected) {
  // Warm both export and allocator state before collecting latency.
  for (int i = 0; i < 2; ++i) deleteMesh(exportMesh(m));
  std::array<double, 7> times;
  for (double& elapsed : times) {
    const auto start = std::chrono::steady_clock::now();
    auto result = exportMesh(m);
    const auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration<double, std::milli>(end - start).count();
    checkEqual(*from_c(result), expected);
    deleteMesh(result);
  }
  std::sort(times.begin(), times.end());
  allocation::begin();
  auto result = exportMesh(m);
  allocation::active = false;
  checkEqual(*from_c(result), expected);
  std::printf("%s,%d,%zu,%zu,%.6f,%zu,%zu,%zu,%zu,%zu\n", kind, segments,
              expected.NumVert(), expected.NumTri(), times[3],
              allocation::total, allocation::peak, allocation::live,
              allocation::count, payloadBytes(expected));
  deleteMesh(result);
}

int main(int argc, char** argv) {
  const int segments = argc > 1 ? std::atoi(argv[1]) : 1024;
  auto sphere = manifold_sphere(1.0, segments);
  auto m = manifold_calculate_normals(sphere, 0, 60.0);
  manifold_delete_manifold(sphere);
  if (manifold_status(m) != MANIFOLD_NO_ERROR) return 1;
  std::puts("entry,segments,vertices,triangles,median_ms,allocated_bytes,peak_requested_bytes,returned_live_bytes,allocations,payload_bytes");
  {
    auto expected = from_c(m)->GetMeshGL();
    measure("mesh32", segments, m, manifold_get_meshgl,
            manifold_delete_meshgl, expected);
    expected = from_c(m)->GetMeshGL(0);
    measure("mesh32_normals", segments, m,
            [](const ManifoldManifold* x) {
              return manifold_get_meshgl_w_normals(x, 0);
            }, manifold_delete_meshgl, expected);
  }
  {
    auto expected = from_c(m)->GetMeshGL64();
    measure("mesh64", segments, m, manifold_get_meshgl64,
            manifold_delete_meshgl64, expected);
    expected = from_c(m)->GetMeshGL64(0);
    measure("mesh64_normals", segments, m,
            [](const ManifoldManifold* x) {
              return manifold_get_meshgl64_w_normals(x, 0);
            }, manifold_delete_meshgl64, expected);
  }
  // Returned buffers must continue to own their data after the input is freed.
  auto result32 = manifold_get_meshgl(m);
  auto result32Normals = manifold_get_meshgl_w_normals(m, 0);
  auto result64 = manifold_get_meshgl64(m);
  auto result64Normals = manifold_get_meshgl64_w_normals(m, 0);
  auto expected32 = from_c(m)->GetMeshGL();
  auto expected32Normals = from_c(m)->GetMeshGL(0);
  auto expected64 = from_c(m)->GetMeshGL64();
  auto expected64Normals = from_c(m)->GetMeshGL64(0);
  manifold_delete_manifold(m);
  checkEqual(*from_c(result32), expected32);
  checkEqual(*from_c(result32Normals), expected32Normals);
  checkEqual(*from_c(result64), expected64);
  checkEqual(*from_c(result64Normals), expected64Normals);
  manifold_delete_meshgl(result32);
  manifold_delete_meshgl(result32Normals);
  manifold_delete_meshgl64(result64);
  manifold_delete_meshgl64(result64Normals);
}
