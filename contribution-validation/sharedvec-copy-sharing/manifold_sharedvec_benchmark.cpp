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


template <class Operation>
void measure(const std::string& outdir, const char* name, Operation operation) {
  operation().Status();
  allocation::bytes = allocation::calls = 0;
  allocation::active = true;
  manifold::Manifold result = operation();
  const auto status = result.Status();
  allocation::active = false;
  const size_t bytes = allocation::bytes, calls = allocation::calls;
  if (status != manifold::Manifold::Error::NoError)
    throw std::runtime_error("Unexpected mesh error");
  std::printf("%s,%zu,%zu,%zu,%zu\n", name, result.NumVert(), result.NumTri(),
              bytes, calls);
  save(outdir + "/" + name + ".bin", result.GetMeshGL64());
}
int main(int argc, char** argv) {
  using namespace manifold;
  if (argc != 3) return 2;
  const std::string outdir = argv[1];
  const int segments = std::atoi(argv[2]);
  std::filesystem::create_directories(outdir);
  const Manifold source = Manifold::Sphere(1, segments).CalculateNormals(0, 180);
  save(outdir + "/source-before.bin", source.GetMeshGL64());
  std::puts("operation,vertices,triangles,requested_malloc_bytes,allocation_calls");
  measure(outdir, "properties", [&] { return source.SetProperties(1, nullptr); });
  measure(outdir, "curvature", [&] { return source.CalculateCurvature(3, 4); });
  measure(outdir, "smoothing", [&] { return source.SmoothByNormals(); });
  measure(outdir, "normals_detach_control", [&] { return source.CalculateNormals(0, 180); });
  measure(outdir, "copy_control", [&] { return Manifold(source); });
  save(outdir + "/source-after.bin", source.GetMeshGL64());
}
