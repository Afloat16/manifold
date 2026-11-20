#include <atomic>
#include <cmath>
#include <iostream>
#include "manifold/manifold.h"
using namespace manifold;
int main() {
  for (int scenario = 0; scenario != 6; ++scenario) {
    vec3 size(2.0);
    double edge = 0.5;
    if (scenario == 1) edge = 3.0;
    if (scenario >= 2 && scenario < 5) size[scenario - 2] = 0.1;
    if (scenario == 5) size.z = 0.0;
    int calls = 0;
    int nonfinite = 0;
    try {
      const Manifold out = Manifold::LevelSet(
          [&](vec3 p) {
            ++calls;
            if (!la::all(la::isfinite(p))) ++nonfinite;
            return 1.0;
          }, Box(vec3(0.0), size), edge, 0.0, -1.0, false);
      std::cout << scenario << "," << calls << "," << nonfinite << ","
                << int(out.Status()) << "," << out.NumTri() << ","
                << out.Volume() << "\n";
    } catch (const std::exception& e) {
      std::cout << scenario << "," << calls << "," << nonfinite
                << ",exception," << e.what() << "\n";
    }
  }
}
