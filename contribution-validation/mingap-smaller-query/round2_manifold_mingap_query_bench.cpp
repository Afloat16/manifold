// Public API Linux/glibc probe: cumulative requested malloc bytes, not peak RSS.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
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
std::string Payload(const MeshGL64& m) {
  std::string out;
  auto append = [&](const auto& values) {
    const uint64_t size = values.size();
    out.append(reinterpret_cast<const char*>(&size), sizeof(size));
    if (!values.empty())
      out.append(reinterpret_cast<const char*>(values.data()),
                 values.size() * sizeof(values[0]));
  };
  out.append(reinterpret_cast<const char*>(&m.numProp), sizeof(m.numProp));
  append(m.vertProperties); append(m.triVerts); append(m.mergeFromVert);
  append(m.mergeToVert); append(m.runIndex); append(m.runOriginalID);
  append(m.runTransform); append(m.runFlags); append(m.faceID);
  append(m.halfedgeTangent);
  return out;
}
struct Case { std::string name; Manifold a, b; double limit, expected; };
int main() {
  const auto cube = Manifold::Cube();
  const auto sphere64 = Manifold::Sphere(1,64).Translate({4,.5,.5});
  const auto sphere256 = Manifold::Sphere(1,256).Translate({4,.5,.5});
  std::vector<Case> cases{
    {"cube_sphere64",cube,sphere64,2.01,2},
    {"cube_sphere256",cube,sphere256,2.01,2},
    {"cube_sphere256_wide",cube,sphere256,3,2},
    {"unequal_spheres",Manifold::Sphere(1,16),Manifold::Sphere(1,128).Translate({4,0,0}),2.01,2},
    {"equal_cubes",cube,cube.Translate({3,0,0}),2.01,2},
    {"equal_spheres",Manifold::Sphere(1,64),Manifold::Sphere(1,64).Translate({4,0,0}),2.01,2},
    {"capped_dense",cube,sphere256,1,1},
    {"overlapping",cube,Manifold::Sphere(1,64).Translate({.5,.5,.5}),.25,0},
    {"contained",cube,Manifold::Sphere(4,64).Translate({.5,.5,.5}),.25,0},
    {"empty",Manifold(),sphere64,2,2},
    {"zero_limit",cube,sphere64,0,0},
    {"negative_limit",cube,sphere64,-.25,.25}
  };
  std::puts("case,reverse,triangles_a,triangles_b,limit,distance,median_us,iqr_us,malloc_bytes,malloc_calls,inputs_unchanged");
  for (const auto& c : cases) {
    const auto beforeA = Payload(c.a.GetMeshGL64());
    const auto beforeB = Payload(c.b.GetMeshGL64());
    for (bool reverse : {false,true}) {
      auto fn = [&] { return reverse ? c.b.MinGap(c.a,c.limit) : c.a.MinGap(c.b,c.limit); };
      fn();
      std::vector<double> samples;
      unsigned long long allocated=0, allocations=0;
      double distance=0;
      for(int run=0;run<11;++run) {
        bytes=0; calls=0;
        const auto start=std::chrono::steady_clock::now();
        recording=true;
        distance=fn();
        recording=false;
        const auto stop=std::chrono::steady_clock::now();
        allocated=bytes; allocations=calls;
        samples.push_back(std::chrono::duration<double,std::micro>(stop-start).count());
        if(!std::isfinite(distance) || std::abs(distance-c.expected)>1e-12) return 2;
      }
      std::sort(samples.begin(),samples.end());
      const bool unchanged=beforeA==Payload(c.a.GetMeshGL64()) && beforeB==Payload(c.b.GetMeshGL64());
      if(!unchanged) return 3;
      std::printf("%s,%d,%zu,%zu,%.17g,%.17g,%.3f,%.3f,%llu,%llu,%d\n",c.name.c_str(),reverse,
        c.a.NumTri(),c.b.NumTri(),c.limit,distance,samples[5],samples[8]-samples[2],allocated,allocations,unchanged);
      std::fflush(stdout);
    }
  }
}
