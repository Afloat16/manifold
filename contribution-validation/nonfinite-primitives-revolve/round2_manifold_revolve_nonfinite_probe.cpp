#include <cstdio>
#include <limits>
#include "manifold/manifold.h"
using namespace manifold;
int main() {
 const double nan=std::numeric_limits<double>::quiet_NaN();
 const double inf=std::numeric_limits<double>::infinity();
 std::puts("axis,input,revolve_status,revolve_volume,revolve_tris,extrude_status");
 for(int axis:{0,1}) for(double v:{nan,inf,-inf}) {
  Polygons p{{{1,0},{2,0},{2,1},{1,1}}};p[0][1][axis]=v;
  auto a=Manifold::Revolve(p,16);auto b=Manifold::Extrude(p,1);
  std::printf("%d,%g,%d,%.17g,%zu,%d\n",axis,v,static_cast<int>(a.Status()),a.Volume(),a.NumTri(),static_cast<int>(b.Status()));
 }
}
