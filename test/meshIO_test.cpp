// Copyright 2026 The Manifold Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "../extras/meshIO.h"

#include <chrono>
#include <filesystem>

#include "gtest/gtest.h"
#include "manifold/manifold.h"

using namespace manifold;

class MeshIOCoordinates : public testing::TestWithParam<const char*> {
 protected:
  std::filesystem::path directory;

  void SetUp() override {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory = std::filesystem::temp_directory_path() /
                ("manifold-meshio-" + std::to_string(stamp));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
  }

  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
  }
};

TEST_P(MeshIOCoordinates, RoundTripPreservesAxes) {
  const Manifold original =
      Manifold::Cube(vec3(1, 2, 4)).Translate(vec3(3, 5, 7));
  const std::string filename =
      (directory / (std::string("mesh.") + GetParam())).string();
  ExportMesh(filename, original.GetMeshGL(), {});
  MeshGL mesh = ImportMesh(filename);
  mesh.Merge();
  const Manifold imported(mesh);

  ASSERT_EQ(imported.Status(), Manifold::Error::NoError);
  const Box expected = original.BoundingBox();
  const Box actual = imported.BoundingBox();
  for (int axis : {0, 1, 2}) {
    EXPECT_DOUBLE_EQ(actual.min[axis], expected.min[axis]);
    EXPECT_DOUBLE_EQ(actual.max[axis], expected.max[axis]);
  }
}

INSTANTIATE_TEST_SUITE_P(Formats, MeshIOCoordinates,
                         testing::Values("obj", "ply", "stl", "glb", "gltf"));
