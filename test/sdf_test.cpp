// Copyright 2022 The Manifold Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <atomic>

#include "gtest/gtest.h"
#include "manifold/manifold.h"
#include "test.h"

using namespace manifold;

struct CubeVoid {
  double operator()(vec3 p) const {
    const vec3 min = p + vec3(1);
    const vec3 max = vec3(1) - p;
    const double min3 = std::min(min.x, std::min(min.y, min.z));
    const double max3 = std::min(max.x, std::min(max.y, max.z));
    return -1.0 * std::min(min3, max3);
  }
};

struct Layers {
  double operator()(vec3 p) const {
    int a = std::fmod(std::round(2 * p.z), 4.0);
    return a == 0 ? 1 : (a == 2 ? -1 : 0);
  }
};

TEST(SDF, CallbackCopies) {
  struct CountedSphere {
    int& copies;
    int& calls;
    CountedSphere(int& copies, int& calls) : copies(copies), calls(calls) {}
    CountedSphere(const CountedSphere& other)
        : copies(other.copies), calls(other.calls) {
      ++copies;
    }
    double operator()(vec3 p) const {
      ++calls;
      return 1 - la::length(p);
    }
  };

  for (const double tolerance : {-1.0, 1e-4}) {
    SCOPED_TRACE(tolerance);
    int copies = 0, calls = 0;
    const Manifold sphere = Manifold::LevelSet(CountedSphere(copies, calls),
                                               Box(vec3(-1.2), vec3(1.2)), 0.1,
                                               0, tolerance, false);
    EXPECT_EQ(sphere.Status(), Manifold::Error::NoError);
    EXPECT_NEAR(sphere.Volume(), 4 * kPi / 3, 0.15);
    ASSERT_GT(calls, 100);
    // Preserve private callback state, but avoid copies for unevaluated paths.
    EXPECT_LT(copies, calls + calls / 10);
  }
}

TEST(SDF, MutableCallbackState) {
  struct StatefulSphere {
    std::atomic<int>& maxCalls;
    int calls = 0;
    bool perturbAfterFirst;
    double operator()(vec3 p) {
      ++calls;
      int previous = maxCalls.load(std::memory_order_relaxed);
      while (previous < calls &&
             !maxCalls.compare_exchange_weak(previous, calls,
                                             std::memory_order_relaxed)) {
      }
      return 1 - la::length(p) + (perturbAfterFirst && calls > 1 ? 0.02 : 0);
    }
  };

  for (const double tolerance : {-1.0, 1e-4}) {
    for (const bool perturb : {false, true}) {
      SCOPED_TRACE(tolerance);
      SCOPED_TRACE(perturb);
      MeshGL64 sequential;
      for (const bool canParallel : {false, true}) {
        std::atomic<int> maxCalls{0};
        const Manifold sphere = Manifold::LevelSet(
            StatefulSphere{maxCalls, 0, perturb}, Box(vec3(-1.2), vec3(1.2)),
            0.1, 0, tolerance, canParallel);
        ASSERT_EQ(sphere.Status(), Manifold::Error::NoError);
        if (tolerance < 0) {
          // Each sample gets fresh state; no callback is needed on edges.
          EXPECT_EQ(maxCalls.load(), 1);
          EXPECT_NEAR(sphere.Volume(), 4 * kPi / 3, 0.08);
        } else {
          // A single root search retains state between its evaluations.
          EXPECT_GT(maxCalls.load(), 1);
        }
        const MeshGL64 mesh = sphere.GetMeshGL64();
        if (!canParallel) {
          sequential = mesh;
        } else {
          EXPECT_EQ(mesh.vertProperties, sequential.vertProperties);
          EXPECT_EQ(mesh.triVerts, sequential.triVerts);
        }
      }
    }
  }
}

TEST(SDF, InvalidGrid) {
  auto sphere = [](vec3 p) { return 1 - la::length(p); };
  const Box box(vec3(-1), vec3(1));
  EXPECT_EQ(Manifold::LevelSet(sphere, box, 0).Status(),
            Manifold::Error::InvalidConstruction);
  EXPECT_EQ(Manifold::LevelSet(sphere, box, -1).Status(),
            Manifold::Error::InvalidConstruction);
  EXPECT_EQ(Manifold::LevelSet(sphere, box, NAN).Status(),
            Manifold::Error::InvalidConstruction);
  Box nanBox = box;
  nanBox.max.x = NAN;
  EXPECT_EQ(Manifold::LevelSet(sphere, nanBox, 0.5).Status(),
            Manifold::Error::InvalidConstruction);
  Box invertedBox = box;
  std::swap(invertedBox.min, invertedBox.max);
  EXPECT_EQ(Manifold::LevelSet(sphere, invertedBox, 0.5).Status(),
            Manifold::Error::InvalidConstruction);
  Box infBox = box;
  infBox.max.x = INFINITY;
  EXPECT_EQ(Manifold::LevelSet(sphere, infBox, 0.5).Status(),
            Manifold::Error::InvalidConstruction);
  EXPECT_EQ(
      Manifold::LevelSet(sphere, Box(vec3(0.0), vec3(3e9, 1, 1)), 1).Status(),
      Manifold::Error::ResultTooLarge);
  EXPECT_EQ(Manifold::LevelSet(sphere, box, 1e-9).Status(),
            Manifold::Error::ResultTooLarge);
  EXPECT_EQ(Manifold::LevelSet(sphere, box, 1e-6).Status(),
            Manifold::Error::ResultTooLarge);
}

TEST(SDF, SphereShell) {
  Manifold sphere = Manifold::LevelSet(
      [](vec3 pos) {
        const double r = la::length(pos);
        return la::min(1 - r, r - 0.995f);
      },
      {vec3(-1.1), vec3(1.1)}, 0.01, 0, 0.0001);

  EXPECT_NEAR(sphere.Genus(), 14235, 1000);

  if (options.exportModels) WriteTestOBJ("sphereShellSDF.obj", sphere);
}

TEST(SDF, CubeVoid) {
  CubeVoid voidSDF;

  EXPECT_EQ(voidSDF({0, 0, 0}), -1);
  EXPECT_EQ(voidSDF({0, 0, 1}), 0);
  EXPECT_EQ(voidSDF({0, 1, 1}), 0);
  EXPECT_EQ(voidSDF({-1, 0, 0}), 0);
  EXPECT_EQ(voidSDF({1, 1, -1}), 0);
  EXPECT_EQ(voidSDF({2, 0, 0}), 1);
  EXPECT_EQ(voidSDF({2, -2, 0}), 1);
  EXPECT_EQ(voidSDF({-2, 2, 2}), 1);
}

TEST(SDF, Bounds) {
  const double size = 4;
  const double edgeLength = 1;

  Manifold cubeVoid = Manifold::LevelSet(
      CubeVoid(), {vec3(-size / 2), vec3(size / 2)}, edgeLength);
  Box bounds = cubeVoid.BoundingBox();
  const double epsilon = cubeVoid.GetEpsilon();
  if (options.exportModels) WriteTestOBJ("cubeVoid.obj", cubeVoid);

  EXPECT_EQ(cubeVoid.Status(), Manifold::Error::NoError);
  EXPECT_EQ(cubeVoid.Genus(), -1);
  const double outerBound = size / 2;
  EXPECT_NEAR(bounds.min.x, -outerBound, epsilon);
  EXPECT_NEAR(bounds.min.y, -outerBound, epsilon);
  EXPECT_NEAR(bounds.min.z, -outerBound, epsilon);
  EXPECT_NEAR(bounds.max.x, outerBound, epsilon);
  EXPECT_NEAR(bounds.max.y, outerBound, epsilon);
  EXPECT_NEAR(bounds.max.z, outerBound, epsilon);
}

TEST(SDF, Bounds2) {
  const double size = 4;
  const double edgeLength = 1;

  Manifold cubeVoid = Manifold::LevelSet(
      CubeVoid(), {vec3(-size / 2), vec3(size / 2)}, edgeLength);
  Box bounds = cubeVoid.BoundingBox();
  const double epsilon = cubeVoid.GetEpsilon();
  if (options.exportModels) WriteTestOBJ("cubeVoid2.obj", cubeVoid);

  EXPECT_EQ(cubeVoid.Status(), Manifold::Error::NoError);
  EXPECT_EQ(cubeVoid.Genus(), -1);
  const double outerBound = size / 2;
  EXPECT_NEAR(bounds.min.x, -outerBound, epsilon);
  EXPECT_NEAR(bounds.min.y, -outerBound, epsilon);
  EXPECT_NEAR(bounds.min.z, -outerBound, epsilon);
  EXPECT_NEAR(bounds.max.x, outerBound, epsilon);
  EXPECT_NEAR(bounds.max.y, outerBound, epsilon);
  EXPECT_NEAR(bounds.max.z, outerBound, epsilon);
}

TEST(SDF, Bounds3) {
  const double radius = 1.2;
  Manifold sphere =
      Manifold::LevelSet([radius](vec3 pos) { return radius - length(pos); },
                         {vec3(-1), vec3(1)}, 0.1);
  if (options.exportModels) WriteTestOBJ("sphere.obj", sphere);

  EXPECT_EQ(sphere.Status(), Manifold::Error::NoError);
  EXPECT_EQ(sphere.Genus(), 0);
  const double epsilon = sphere.GetEpsilon();
  Box bounds = sphere.BoundingBox();
  EXPECT_NEAR(bounds.min.x, -1, epsilon);
  EXPECT_NEAR(bounds.min.y, -1, epsilon);
  EXPECT_NEAR(bounds.min.z, -1, epsilon);
  EXPECT_NEAR(bounds.max.x, 1, epsilon);
  EXPECT_NEAR(bounds.max.y, 1, epsilon);
  EXPECT_NEAR(bounds.max.z, 1, epsilon);
}

TEST(SDF, Void) {
  const double size = 4;
  const double edgeLength = 0.5;

  Manifold cubeVoid = Manifold::LevelSet(
      CubeVoid(), {vec3(-size / 2), vec3(size / 2)}, edgeLength);

  Manifold cube = Manifold::Cube(vec3(size), true);
  cube -= cubeVoid;
  Box bounds = cube.BoundingBox();
  const double epsilon = cube.GetEpsilon();
  if (options.exportModels) WriteTestOBJ("cube.obj", cube);

  EXPECT_EQ(cubeVoid.Status(), Manifold::Error::NoError);
  EXPECT_EQ(cube.Genus(), 0);
  EXPECT_NEAR(cube.Volume(), 8, 0.001);
  EXPECT_NEAR(cube.SurfaceArea(), 24, 0.001);
  EXPECT_NEAR(bounds.min.x, -1, epsilon);
  EXPECT_NEAR(bounds.min.y, -1, epsilon);
  EXPECT_NEAR(bounds.min.z, -1, epsilon);
  EXPECT_NEAR(bounds.max.x, 1, epsilon);
  EXPECT_NEAR(bounds.max.y, 1, epsilon);
  EXPECT_NEAR(bounds.max.z, 1, epsilon);
}

TEST(SDF, Resize) {
  const double size = 20;
  Manifold layers = Manifold::LevelSet(Layers(), {vec3(0.0), vec3(size)}, 1);
  if (options.exportModels) WriteTestOBJ("layers.obj", layers);

  EXPECT_EQ(layers.Status(), Manifold::Error::NoError);
  EXPECT_EQ(layers.Genus(), -8);
  const double epsilon = layers.GetEpsilon();
  Box bounds = layers.BoundingBox();
  EXPECT_NEAR(bounds.min.x, 0, epsilon);
  EXPECT_NEAR(bounds.min.y, 0, epsilon);
  EXPECT_NEAR(bounds.min.z, 1.5, epsilon);
  EXPECT_NEAR(bounds.max.x, size, epsilon);
  EXPECT_NEAR(bounds.max.y, size, epsilon);
  EXPECT_NEAR(bounds.max.z, size - 1.5, epsilon);
}

TEST(SDF, SineSurface) {
  Manifold surface = Manifold::LevelSet(
      [](vec3 p) {
        double mid = la::sin(p.x) + la::sin(p.y);
        return (p.z > mid - 0.5 && p.z < mid + 0.5) ? 1.0f : -1.0f;
      },
      {vec3(-1.75 * kPi), vec3(1.75 * kPi)}, 1);

  EXPECT_EQ(surface.Status(), Manifold::Error::NoError);
  EXPECT_EQ(surface.Genus(), 38);
  EXPECT_NEAR(surface.Volume(), 102.4, 0.1);
  EXPECT_NEAR(surface.SurfaceArea(), 392.4, 0.1);

  if (options.exportModels) WriteTestOBJ("sinesurface.obj", surface);
}

TEST(SDF, Determinism) {
  const auto gyroid = [](vec3 p) {
    return la::sin(p.x) * la::cos(p.y) + la::sin(p.y) * la::cos(p.z) +
           la::sin(p.z) * la::cos(p.x);
  };
  const Box bounds = {vec3(-10), vec3(10)};
  const MeshGL first = Manifold::LevelSet(gyroid, bounds, 1.0).GetMeshGL();
  for (int i = 0; i < 10; ++i) {
    const MeshGL mesh = Manifold::LevelSet(gyroid, bounds, 1.0).GetMeshGL();
    ASSERT_EQ(mesh.vertProperties, first.vertProperties);
    ASSERT_EQ(mesh.triVerts, first.triVerts);
  }
}

TEST(SDF, Blobs) {
  const double blend = 1;
  std::vector<vec4> balls = {{0, 0, 0, 2},     //
                             {1, 2, 3, 2},     //
                             {-2, 2, -2, 1},   //
                             {-2, -3, -2, 2},  //
                             {-3, -1, -3, 1},  //
                             {2, -3, -2, 2},   //
                             {-2, 3, 2, 2},    //
                             {-2, -3, 2, 2},   //
                             {1, -1, 1, -2},   //
                             {-4, -3, -2, 1}};
  Manifold blobs = Manifold::LevelSet(
      [&balls, blend](vec3 p) {
        double d = 0;
        for (const auto& ball : balls) {
          d += (ball.w > 0 ? 1 : -1) *
               smoothstep(-blend, blend,
                          std::abs(ball.w) - la::length(vec3(ball) - p));
        }
        return d;
      },
      {vec3(-5), vec3(5)}, 0.05, 0.5);

  const int chi = blobs.NumVert() - blobs.NumTri() / 2;
  const int genus = 1 - chi / 2;
  EXPECT_EQ(genus, 0);

  if (options.exportModels) WriteTestOBJ("blobs.obj", blobs);
}
