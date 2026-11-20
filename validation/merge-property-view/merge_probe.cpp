#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "manifold/manifold.h"

extern "C" void* __libc_malloc(size_t);
static bool measuring = false;
static size_t allocBytes = 0, allocCount = 0;
extern "C" void* malloc(size_t bytes) {
  if (measuring) { allocBytes += bytes; ++allocCount; }
  return __libc_malloc(bytes);
}
using namespace manifold;

template<class V> void Hash(uint64_t& hash, const V& values) {
  const unsigned char* p = reinterpret_cast<const unsigned char*>(values.data());
  for (size_t i=0; i < values.size()*sizeof(typename V::value_type); ++i) {
    hash ^= p[i]; hash *= 1099511628211ull;
  }
}
template<class M> uint64_t HashMesh(const M& mesh) {
  uint64_t hash = 1469598103934665603ull;
  Hash(hash, mesh.vertProperties); Hash(hash, mesh.triVerts);
  Hash(hash, mesh.mergeFromVert); Hash(hash, mesh.mergeToVert);
  Hash(hash, mesh.runIndex); Hash(hash, mesh.runOriginalID);
  Hash(hash, mesh.runTransform); Hash(hash, mesh.runFlags);
  Hash(hash, mesh.faceID); Hash(hash, mesh.halfedgeTangent);
  return hash;
}
template<class M> void Run(const char* precision, int copies, int props, bool closed) {
  auto cube = Manifold::Cube().GetMeshGL64();
  M mesh;
  mesh.numProp = props;
  for(int c=0;c<copies;++c) {
    auto add = [&](size_t vert) {
      const auto n = mesh.NumVert();
      for(int k=0;k<3;++k) mesh.vertProperties.push_back(cube.vertProperties[vert*cube.numProp+k]+(k==0 ? 3.0*c : 0.0));
      for(int k=3;k<props;++k) mesh.vertProperties.push_back(n*0.125+k*0.25);
    };
    if(closed) {
      const auto start=mesh.NumVert();
      for(size_t v=0;v<cube.NumVert();++v) add(v);
      for(auto v:cube.triVerts) mesh.triVerts.push_back(start+v);
    } else {
      for(auto v:cube.triVerts) { mesh.triVerts.push_back(mesh.NumVert()); add(v); }
    }
  }
  const auto inputHash=HashMesh(mesh);
  auto reference=mesh.Merge();
  const auto outputHash=HashMesh(reference);
  allocBytes=allocCount=0;
  measuring=true;
  auto output=mesh.Merge();
  measuring=false;
  if(HashMesh(output)!=outputHash || HashMesh(mesh)!=inputHash || output.vertProperties!=mesh.vertProperties) std::abort();
  std::vector<double> samples;
  for(int r=0;r<7;++r) {
    const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<5;++i) { auto result=mesh.Merge(); if(result.mergeFromVert.size()!=reference.mergeFromVert.size()) std::abort(); }
    const auto end=std::chrono::steady_clock::now();
    samples.push_back(std::chrono::duration<double,std::micro>(end-start).count()/5);
  }
  std::sort(samples.begin(),samples.end());
  std::printf("%s,%d,%d,%d,%zu,%zu,%llu,%llu,%.3f\n",precision,copies,props,closed,allocCount,allocBytes,(unsigned long long)inputHash,(unsigned long long)outputHash,samples[3]);
}
int main() {
  std::printf("precision,copies,properties,closed,malloc_count,malloc_bytes,input_hash,output_hash,microseconds\n");
  for(int copies:{1,100,1000}) for(int props:{3,9,64}) {
    Run<MeshGL>("float",copies,props,false);
    Run<MeshGL64>("double",copies,props,false);
  }
  Run<MeshGL>("float",1000,64,true);
  Run<MeshGL64>("double",1000,64,true);
}
