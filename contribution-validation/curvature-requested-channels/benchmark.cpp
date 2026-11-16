#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "manifold/manifold.h"

// Linux/glibc-only native validation. Counts allocations in the shared library.
// PAR must be OFF. No process-RSS or latency claim is made.
namespace allocation {
bool active = false;
size_t bytes = 0, calls = 0;
}
extern "C" void* __libc_malloc(size_t);
extern "C" void* __libc_calloc(size_t, size_t);
extern "C" void* __libc_realloc(void*, size_t);
extern "C" void* malloc(size_t n) {
  void* result = __libc_malloc(n);
  if (allocation::active) { allocation::bytes += n; ++allocation::calls; }
  return result;
}
extern "C" void* calloc(size_t n, size_t width) {
  void* result = __libc_calloc(n, width);
  if (allocation::active) { allocation::bytes += n * width; ++allocation::calls; }
  return result;
}
extern "C" void* realloc(void* p, size_t n) {
  void* result = __libc_realloc(p, n);
  if (allocation::active) { allocation::bytes += n; ++allocation::calls; }
  return result;
}
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




#include <algorithm>
#include <array>
#include <ctime>

uint64_t cpuTime() {
  timespec t;
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t))
    throw std::runtime_error("Cannot read thread CPU time");
  return uint64_t(t.tv_sec) * 1000000000 + t.tv_nsec;
}
void measure(const std::string& outdir, const char* name,
             const manifold::Manifold& source, int gaussian, int mean) {
  using namespace manifold;
  if (source.Status() != Manifold::Error::NoError)
    throw std::runtime_error("Invalid source");
  allocation::bytes = allocation::calls = 0;
  allocation::active = true;
  const Manifold result = source.CalculateCurvature(gaussian, mean);
  const auto status = result.Status();
  allocation::active = false;
  const size_t bytes = allocation::bytes, allocations = allocation::calls;
  if (status != Manifold::Error::NoError || result.NumVert() != source.NumVert())
    throw std::runtime_error("Unexpected curvature result");
  std::array<uint64_t, 7> times;
  for (auto& t : times) {
    const auto start = cpuTime();
    {
      const Manifold r = source.CalculateCurvature(gaussian, mean);
      if (r.Status() != Manifold::Error::NoError) throw std::runtime_error("Invalid result");
    }
    t = cpuTime() - start;
  }
  std::sort(times.begin(), times.end());
  std::printf("%s,%zu,%zu,%zu,%zu,%llu\n", name, source.NumVert(), source.NumTri(),
              bytes, allocations, static_cast<unsigned long long>(times[3]));
  save(outdir + "/" + name + ".bin", result.GetMeshGL64());
}
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  using namespace manifold;
  const std::string outdir = argv[1];
  std::filesystem::create_directories(outdir);
  const Manifold sphere = Manifold::Sphere(1, 512);
  const Manifold colored = Manifold::Sphere(1, 128).SetProperties(
      3, [](double* p, vec3 v, const double*) { for (int i=0;i<3;++i) p[i]=10+i+v[i]; });
  const Manifold concave = Manifold::Sphere(1, 64) -
      Manifold::Cube(vec3(1), true).Translate({0.8, 0, 0});
  const Manifold flat = Manifold::Cube(vec3(1), true).Refine(16);
  save(outdir + "/sphere-input-before.bin", sphere.GetMeshGL64());
  save(outdir + "/colored-input-before.bin", colored.GetMeshGL64());
  std::puts("case,vertices,triangles,requested_malloc_bytes,allocation_calls,median_thread_cpu_ns_7_repetitions");
  measure(outdir, "sphere_mean", sphere, -1, 0);
  measure(outdir, "sphere_gaussian", sphere, 0, -1);
  measure(outdir, "sphere_both", sphere, 0, 1);
  measure(outdir, "sphere_aliased", sphere, 0, 0);
  measure(outdir, "sphere_neither", sphere, -1, -1);
  measure(outdir, "colored_mean", colored, -1, 1);
  measure(outdir, "colored_gaussian", colored, 1, -1);
  measure(outdir, "concave_mean", concave, -1, 0);
  measure(outdir, "concave_gaussian", concave, 0, -1);
  measure(outdir, "flat_mean", flat, -1, 0);
  measure(outdir, "flat_gaussian", flat, 0, -1);
  measure(outdir, "empty", Manifold(), 0, 1);
  save(outdir + "/sphere-input-after.bin", sphere.GetMeshGL64());
  save(outdir + "/colored-input-after.bin", colored.GetMeshGL64());
}
