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



#include <array>
struct Stats { size_t copies = 0, calls = 0; };
struct Field {
  Stats& stats;
  std::array<double, 16> matrix;
  bool plane;
  Field(Stats& stats, bool plane) : stats(stats), plane(plane) {
    matrix = {1.1, 0, 0, 0, 0, 0.9, 0, 0, 0, 0, 1.2, 0, 0.05, 0, 0, 1};
  }
  Field(const Field& other)
      : stats(other.stats), matrix(other.matrix), plane(other.plane) {
    ++stats.copies;
  }
  Field(Field&& other) = default;
  double operator()(manifold::vec3 p) const {
    ++stats.calls;
    using namespace manifold;
    const double w = matrix[3]*p.x + matrix[7]*p.y + matrix[11]*p.z + matrix[15];
    const vec3 transformed((matrix[0]*p.x + matrix[4]*p.y + matrix[8]*p.z + matrix[12])/w,
                           (matrix[1]*p.x + matrix[5]*p.y + matrix[9]*p.z + matrix[13])/w,
                           (matrix[2]*p.x + matrix[6]*p.y + matrix[10]*p.z + matrix[14])/w);
    return plane ? transformed.x : 1.0 - la::length(transformed);
  }
};
void sample(const std::string& outdir, const char* name, double spacing,
            double tolerance, bool plane, double level = 0) {
  using namespace manifold;
  Stats stats;
  const std::function<double(vec3)> sdf = Field(stats, plane);
  stats.copies = stats.calls = 0;
  allocation::bytes = allocation::calls = 0;
  allocation::active = true;
  const Manifold result = Manifold::LevelSet(sdf, Box(vec3(-1.2),vec3(1.2)),
                                            spacing, level, tolerance, false);
  const auto status = result.Status();
  allocation::active = false;
  const size_t bytes = allocation::bytes, allocations = allocation::calls;
  if (status != Manifold::Error::NoError) throw std::runtime_error("Unexpected level-set error");
  std::printf("%s,%zu,%zu,%zu,%zu,%zu,%zu\n", name, result.NumVert(), result.NumTri(),
              stats.copies, stats.calls, bytes, allocations);
  save(outdir + "/" + name + ".bin", result.GetMeshGL64());
}
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string outdir = argv[1];
  std::filesystem::create_directories(outdir);
  std::puts("case,vertices,triangles,callback_copies,evaluations,requested_malloc_bytes,allocation_calls");
  sample(outdir, "coarse", .2, -1, false);
  sample(outdir, "medium", .1, -1, false);
  sample(outdir, "fine", .05, -1, false);
  sample(outdir, "accurate", .1, 1e-5, false);
  sample(outdir, "plane", .1, 1e-5, true);
  sample(outdir, "empty", .1, -1, false, 2);
}
