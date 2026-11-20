// Public-API Linux/glibc probe. Counts cumulative requested malloc bytes,
// not peak memory. Compare complete payload files between base and patch.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include "manifold/manifold.h"

static std::atomic<bool> recording{false};
static std::atomic<unsigned long long> bytes{0}, calls{0};
extern "C" void* __libc_malloc(size_t) noexcept;
extern "C" void* malloc(size_t n) noexcept {
  void* p = __libc_malloc(n);
  if (recording.load(std::memory_order_relaxed)) {
    bytes.fetch_add(n, std::memory_order_relaxed);
    calls.fetch_add(1, std::memory_order_relaxed);
  }
  return p;
}
using namespace manifold;

std::string Payload(const MeshGL64& mesh) {
  std::string result;
  auto append = [&](const auto& data) {
    const uint64_t count = data.size();
    result.append(reinterpret_cast<const char*>(&count), sizeof(count));
    if (!data.empty())
      result.append(reinterpret_cast<const char*>(data.data()),
                    data.size() * sizeof(data[0]));
  };
  result.append(reinterpret_cast<const char*>(&mesh.numProp), sizeof(mesh.numProp));
  append(mesh.vertProperties); append(mesh.triVerts);
  append(mesh.mergeFromVert); append(mesh.mergeToVert);
  append(mesh.runIndex); append(mesh.runOriginalID); append(mesh.runTransform);
  append(mesh.runFlags); append(mesh.faceID); append(mesh.halfedgeTangent);
  return result;
}
uint64_t Hash(const std::string& value) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : value) h = (h ^ c) * 1099511628211ull;
  return h;
}
struct Case { std::string name; Manifold input; int normalIndex = 0; };
int main(int argc, char** argv) {
  if (argc != 2) return 1;
  std::vector<Case> cases;
  for (int n : {16, 64, 256}) {
    cases.push_back({"sphere_" + std::to_string(n),
                     Manifold::Sphere(1, n).CalculateNormals(0, 180)});
  }
  cases.push_back({"missing_normals", Manifold::Sphere(1, 64)});
  cases.push_back({"partial_normals", Manifold::Sphere(1, 64).SetProperties(
      3, [](double* out, vec3 pos, const double*) {
        for (int k : {0, 1, 2}) out[k] = pos.z > 0 ? pos[k] : 0;
      })});
  cases.push_back({"faceted_cube", Manifold::Cube().CalculateNormals(0, 0)});
  cases.push_back({"creased_cylinder", Manifold::Cylinder(3, 2, 1, 128).CalculateNormals()});
  cases.push_back({"high_valence_cone", Manifold::Cylinder(3, 2, 0, 512).CalculateNormals(0, 180)});
  cases.push_back({"high_then_low", (Manifold::Cylinder(3, 2, 0, 512) +
                                      Manifold::Cube().Translate({5, 0, 0})).CalculateNormals(0, 180)});
  cases.push_back({"existing_tangents", Manifold::Sphere(1, 64).CalculateNormals(0, 180).SmoothByNormals()});
  cases.push_back({"empty", Manifold()});
  std::puts("case,vertices,triangles,median_us,iqr_us,malloc_bytes,malloc_calls,payload_hash,input_unchanged,status");
  for (const auto& c : cases) {
    const auto before = Payload(c.input.GetMeshGL64());
    const Manifold alias = c.input;
    c.input.SmoothByNormals(c.normalIndex).NumTri();
    std::string payload;
    std::vector<double> timings;
    unsigned long long allocated = 0, allocations = 0;
    int status = -1;
    for (int run = 0; run < 7; ++run) {
      bytes = 0; calls = 0;
      auto start = std::chrono::steady_clock::now();
      recording = true;
      Manifold result = c.input.SmoothByNormals(c.normalIndex);
      recording = false;
      auto end = std::chrono::steady_clock::now();
      allocated = bytes; allocations = calls;
      status = static_cast<int>(result.Status());
      const auto current = Payload(result.GetMeshGL64());
      if (run > 0 && payload != current) return 2;
      payload = current;
      timings.push_back(std::chrono::duration<double,std::micro>(end-start).count());
    }
    std::sort(timings.begin(), timings.end());
    const bool unchanged = before == Payload(alias.GetMeshGL64());
    if (!unchanged || status != 0) return 3;
    std::ofstream out(std::string(argv[1]) + "/" + c.name + ".bin", std::ios::binary);
    out.write(payload.data(), payload.size());
    std::ofstream src(std::string(argv[1]) + "/" + c.name + "-input.bin", std::ios::binary);
    src.write(before.data(), before.size());
    if (!out || !src) return 4;
    std::printf("%s,%zu,%zu,%.3f,%.3f,%llu,%llu,%016llx,%d,%d\n", c.name.c_str(),
      c.input.NumVert(), c.input.NumTri(), timings[3],timings[5]-timings[1],allocated,allocations,
      static_cast<unsigned long long>(Hash(payload)), unchanged, status);
  }
}
