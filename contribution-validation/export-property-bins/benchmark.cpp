#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include "manifold/manifold.h"

// Requested C++ allocations during an export, not process RSS. PAR must be OFF.
namespace allocation {
struct alignas(std::max_align_t) Header { size_t size, generation; };
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
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, size_t) noexcept { ::operator delete(p); }

template <typename T>
void writeVector(std::ofstream& out, const std::vector<T>& v) {
  const uint64_t size = v.size();
  out.write(reinterpret_cast<const char*>(&size), sizeof(size));
  out.write(reinterpret_cast<const char*>(v.data()), v.size() * sizeof(T));
}
template <typename Mesh>
void save(const std::string& path, const Mesh& mesh) {
  std::ofstream out(path, std::ios::binary);
  const uint64_t numProp = mesh.numProp;
  out.write(reinterpret_cast<const char*>(&numProp), sizeof(numProp));
  writeVector(out, mesh.vertProperties);
  writeVector(out, mesh.triVerts);
  writeVector(out, mesh.mergeFromVert);
  writeVector(out, mesh.mergeToVert);
  writeVector(out, mesh.runIndex);
  writeVector(out, mesh.runOriginalID);
  writeVector(out, mesh.runTransform);
  writeVector(out, mesh.runFlags);
  writeVector(out, mesh.faceID);
  writeVector(out, mesh.halfedgeTangent);
  if (!out) throw std::runtime_error("Cannot save export");
}

template <typename Export>
void measure(const std::string& outdir, const std::string& name,
             Export exportMesh) {
  for (int i = 0; i < 2; ++i) exportMesh();
  std::array<double, 7> times;
  for (double& elapsed : times) {
    const auto start = std::chrono::steady_clock::now();
    auto result = exportMesh();
    const auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration<double, std::milli>(end - start).count();
  }
  std::sort(times.begin(), times.end());
  allocation::begin();
  auto result = exportMesh();
  allocation::active = false;
  std::printf("%s,%zu,%zu,%zu,%zu,%zu,%.6f,%zu,%zu,%zu,%zu\n",
              name.c_str(), size_t(result.NumVert()), size_t(result.NumTri()),
              result.mergeFromVert.size(), result.runOriginalID.size(),
              result.halfedgeTangent.size(), times[3], allocation::total,
              allocation::peak, allocation::live, allocation::count);
  save(outdir + "/" + name + ".bin", result);
}

int main(int argc, char** argv) {
  using namespace manifold;
  if (argc < 2) return 2;
  const std::string outdir = argv[1];
  const int segments = argc > 2 ? std::atoi(argv[2]) : 1024;
  std::filesystem::create_directories(outdir);
  const Manifold sphere = Manifold::Sphere(1, segments);
  const Manifold small = Manifold::Sphere(1, 64);
  const Manifold cube = Manifold::Cube(vec3(1), true);
  std::vector<std::pair<std::string, Manifold>> cases;
  cases.emplace_back("no_properties", sphere);
  cases.emplace_back("smooth_normals", sphere.CalculateNormals(0));
  cases.emplace_back("faceted_normals", Manifold::Sphere(1, 256).CalculateNormals(0, 0));
  cases.emplace_back("cube_seams", cube.CalculateNormals(0, 0));
  cases.emplace_back("boolean_seams", (small - small.Translate({0.5, 0.1, 0.2})).CalculateNormals(0));
  cases.emplace_back("smooth_tangents", small.CalculateNormals(0).SmoothByNormals());
  cases.emplace_back("many_properties", small.SetProperties(11, [](double* out, vec3 p, const double*) {
    for (int i = 0; i < 11; ++i) out[i] = p[i % 3] * (i + 1);
  }));
  cases.emplace_back("constant_properties", small.SetProperties(3, [](double* out, vec3, const double*) {
    out[0] = 0.25; out[1] = 0.5; out[2] = 0.75;
  }).AsOriginal());
  cases.emplace_back("mixed_runs", Manifold::BatchBoolean(
      {small.CalculateNormals(0), cube.Translate({5, 0, 0})}, OpType::Add));
  cases.emplace_back("transformed_runs", (cube.CalculateNormals(0) + cube.CalculateNormals(0).Translate({0.5, 0.5, 0})).Scale({2, 1, 0.5}).Rotate(10, 20, 30));
  cases.emplace_back("empty", Manifold());
  std::puts("case,vertices,triangles,merges,runs,tangents,median_ms,allocated_bytes,peak_requested_bytes,returned_live_bytes,allocations");
  for (const auto& [name, m] : cases) {
    if (m.Status() != Manifold::Error::NoError) throw std::runtime_error(name);
    measure(outdir, name + "_32", [&m] { return m.GetMeshGL(); });
    measure(outdir, name + "_64", [&m] { return m.GetMeshGL64(); });
    if (m.NumProp() >= 3) {
      measure(outdir, name + "_32_normals", [&m] { return m.GetMeshGL(0); });
      measure(outdir, name + "_64_normals", [&m] { return m.GetMeshGL64(0); });
    }
  }
}
