#include "gtest/gtest.h"
#include "manifold/manifold.h"
using namespace manifold;
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
                                               Box(vec3(-1.2), vec3(1.2)), 0.2,
                                               0, tolerance, false);
    EXPECT_EQ(sphere.Status(), Manifold::Error::NoError);
    EXPECT_NEAR(sphere.Volume(), 4 * kPi / 3, 0.15);
    ASSERT_GT(calls, 100);
    // A callback may be copied for a stage, but not for every sample or edge.
    EXPECT_LT(copies, calls / 10);
  }
}

