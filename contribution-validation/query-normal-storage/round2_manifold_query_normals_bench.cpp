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

uint64_t Hash(const std::vector<int>& values) {
  uint64_t h = 1469598103934665603ULL;
  for (int v : values) {
    h ^= static_cast<uint32_t>(v);
    h *= 1099511628211ULL;
  }
  return h;
}
uint64_t Hash(const std::vector<RayHit>& values) {
  uint64_t h = 1469598103934665603ULL;
  auto add = [&](const auto& v) {
    const auto* p = reinterpret_cast<const unsigned char*>(&v);
    for (size_t i = 0; i < sizeof(v); ++i) { h ^= p[i]; h *= 1099511628211ULL; }
  };
  for (const RayHit& hit : values) {
    add(hit.faceID); add(hit.distance); add(hit.crossing);
    for (int a : {0,1,2}) { add(hit.position[a]); add(hit.normal[a]); }
  }
  return h;
}
template <typename Fn, typename Check>
void Record(const std::string& name, const Manifold& mesh, size_t n, Fn fn, Check check) {
  const auto payload = Payload(mesh.GetMeshGL64());
  fn();
  std::vector<double> samples;
  unsigned long long allocated = 0, allocations = 0;
  uint64_t hash = 0;
  size_t size = 0;
  for (int run=0; run<11; ++run) {
    bytes=0; calls=0;
    const auto start=std::chrono::steady_clock::now();
    recording=true;
    const auto result=fn();
    recording=false;
    const auto stop=std::chrono::steady_clock::now();
    allocated=bytes; allocations=calls;
    samples.push_back(std::chrono::duration<double,std::micro>(stop-start).count());
    if (!check(result)) std::exit(2);
    const auto current=Hash(result);
    if (run && current!=hash) std::exit(3);
    hash=current; size=result.size();
  }
  if (payload != Payload(mesh.GetMeshGL64())) std::exit(4);
  std::sort(samples.begin(), samples.end());
  std::printf("%s,%zu,%zu,%zu,%llu,%.3f,%.3f,%llu,%llu,1\n",name.c_str(),mesh.NumTri(),n,size,
    static_cast<unsigned long long>(hash),samples[5],samples[8]-samples[2],allocated,allocations);
  std::fflush(stdout);
}
int main() {
  const Manifold cube=Manifold::Cube(vec3(2.0),true);
  const Manifold dense=Manifold::Sphere(1.0,64);
  const Manifold shell=Manifold::Cube(vec3(4.0),true)-cube;
  const Manifold empty;
  std::puts("case,mesh_triangles,queries,output_size,payload_hash,median_us,iqr_us,malloc_bytes,malloc_calls,mesh_unchanged");
  for (size_t n : {size_t(0),size_t(1),size_t(128),size_t(16384),size_t(131072)}) {
    for (int mode : {0,1,2}) {
      std::vector<vec3> points(n);
      std::vector<int> expected(n);
      for (size_t i=0;i<n;++i) {
        const bool inside=mode==0 || (mode==1 && i%2==0);
        points[i]=inside?vec3((double(i%31)-15)/100,(double(i%29)-14)/100,(double(i%23)-11)/100):vec3(3.0);
        expected[i]=inside?1:0;
      }
      const auto original=points;
      const char* kind=mode==0?"inside":mode==1?"mixed":"outside";
      Record(std::string("winding_")+kind+"_"+std::to_string(n),cube,n,[&]{return cube.WindingNumber(points);},[&](const auto& w){return w==expected;});
      if(points!=original) return 5;
    }
  }
  const std::vector<vec3> sample={{0,0,0},{0,0,1.5},{0,0,3},{.3,.2,.1},{.5,.5,.5},{1,1,1}};
  Record("winding_shell",shell,sample.size(),[&]{return shell.WindingNumber(sample);},[&](const auto& w){return w.size()==sample.size()&&w[0]==0&&w[1]==1&&w[2]==0;});
  Record("winding_dense",dense,sample.size(),[&]{return dense.WindingNumber(sample);},[&](const auto& w){return w.size()==sample.size()&&w[0]==1&&w[1]==0&&w[2]==0;});
  Record("winding_empty",empty,sample.size(),[&]{return empty.WindingNumber(sample);},[&](const auto& w){return w==std::vector<int>(sample.size(),0);});
  struct Ray {const char* name; Manifold mesh; vec3 a,b; int crossings;};
  const std::vector<Ray> rays={
    {"ray_cube_x",cube,{-2,.21,.32},{2,.21,.32},0},
    {"ray_cube_z",cube,{.21,.32,-2},{.21,.32,2},0},
    {"ray_cube_inside",cube,{.21,.32,0},{.21,.32,2},-1},
    {"ray_cube_miss",cube,{-2,3,3},{2,3,3},0},
    {"ray_zero_length",cube,{0,0,0},{0,0,0},0},
    {"ray_shared_vertex",cube,{-2,-2,-2},{2,2,2},0},
    {"ray_shell",shell,{.21,.32,-3},{.21,.32,3},0},
    {"ray_dense",dense,{.21,.32,-2},{.21,.32,2},0},
    {"ray_empty",empty,{-2,0,0},{2,0,0},0}
  };
  for(const auto& ray:rays) Record(ray.name,ray.mesh,1,[&]{return ray.mesh.RayCast(ray.a,ray.b);},[&](const auto& hits){
    int crossing=0;double previous=-1;
    for(const RayHit& hit:hits){if(!std::isfinite(hit.distance)||hit.distance<previous||hit.distance<0||hit.distance>1||!la::all(la::isfinite(hit.normal)))return false;crossing+=hit.crossing;previous=hit.distance;}
    return crossing==ray.crossings;
  });
}
