#pragma once
#include <nanoreflex/polyhedral_surface_2.hpp>

namespace nanoreflex {

struct surface_mesh_curve {
  void clear() {
    face_strip.clear();
    edge_weights.clear();
  }

  bool remove_artifacts(uint32 f) {
    const auto fid = face_strip.back();
    if (f == fid) return true;
    if (face_strip.size() >= 2) {
      const auto fid2 = face_strip[face_strip.size() - 2];
      if (f == fid2) {
        face_strip.pop_back();
        edge_weights.pop_back();
        return true;
      }
    }
    return false;
  }

  void remove_closed_artifacts() {
    if (face_strip.size() < 4) return;
    if (face_strip.front() != face_strip.back()) return;
    int cut = 0;
    while (true) {
      const auto fid1 = face_strip[1 + cut];
      const auto fid2 = face_strip[face_strip.size() - 2 - cut];
      if (fid1 != fid2) break;
      ++cut;
    }
    if (cut == 0) return;
    vector<uint32> new_face_strip(begin(face_strip) + cut,
                                  end(face_strip) - cut);
    face_strip.swap(new_face_strip);
    vector<float32> new_edge_weights(begin(edge_weights) + cut,
                                     end(edge_weights) - cut);
    edge_weights.swap(new_edge_weights);
  }

  bool closed() const { return face_strip.front() == face_strip.back(); }

  vector<uint32> face_strip;
  vector<float32> edge_weights;
};

constexpr auto points_from(const polyhedral_surface& surface,
                           const surface_mesh_curve& curve) -> vector<vec3> {
  vector<vec3> points{};
  if (curve.face_strip.size() < 2) return points;

  points.reserve(curve.face_strip.size());
  points.resize(curve.face_strip.size() - 1);

  for (size_t i = 0; i < curve.face_strip.size() - 1; ++i) {
    const auto e =
        surface.common_edge(curve.face_strip[i], curve.face_strip[i + 1]);
    points[i] = surface.position(e, curve.edge_weights[i]);
  }
  if (curve.closed()) points.push_back(points.front());

  return points;
}

}  // namespace nanoreflex
