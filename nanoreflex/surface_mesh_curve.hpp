#pragma once
#include <nanoreflex/polyhedral_surface_2.hpp>

namespace nanoreflex {

struct surface_mesh_curve {
  void clear() {
    face_strip.clear();
    edge_weights.clear();
  }

  vector<uint32> face_strip;
  vector<float32> edge_weights;
};

constexpr auto points_from(const polyhedral_surface& surface,
                           const surface_mesh_curve& curve) -> vector<vec3> {
  vector<vec3> points{};
  if (curve.face_strip.size() < 2) return points;

  points.resize(curve.face_strip.size() - 1);

  for (size_t i = 0; i < curve.face_strip.size() - 1; ++i) {
    const auto e =
        surface.common_edge(curve.face_strip[i], curve.face_strip[i + 1]);
    points[i] = surface.position(e, curve.edge_weights[i]);
  }
  return points;
}

}  // namespace nanoreflex
