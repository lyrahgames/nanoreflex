#pragma once
#include <nanoreflex/polyhedral_surface_2.hpp>

namespace nanoreflex {

struct surface_mesh_curve {
  void clear() {
    face_strip.clear();
    edge_weights.clear();
    control_points.clear();
  }

  void generate_control_points(const polyhedral_surface& surface) {
    if (face_strip.size() < 2) return;

    control_points.reserve(face_strip.size());
    control_points.resize(face_strip.size() - 1);

    for (size_t i = 0; i < face_strip.size() - 1; ++i) {
      const auto e = surface.common_edge(face_strip[i], face_strip[i + 1]);
      control_points[i] = surface.position(e, edge_weights[i]);
    }
    if (closed()) control_points.push_back(control_points.front());
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

  void smooth(const polyhedral_surface& surface) {
    if (face_strip.size() < 4) return;

    generate_control_points(surface);

    const auto relax = [&](const auto& l, const auto& r, const auto& v1,
                           const auto& v2, float t0) {
      const auto p = r - v1;
      const auto q = l - v1;
      const auto v = v2 - v1;
      const auto vl = length(v);
      const auto ivl = 1 / vl;
      const auto vn = ivl * v;

      const auto py = dot(p, vn);
      const auto qy = dot(q, vn);
      const auto px = -length(p - py * vn);
      const auto qx = length(q - qy * vn);

      const auto t = (py * qx - qy * px) / (qx - px) * ivl;
      const auto relaxation = 0.2f;
      return std::clamp((1 - relaxation) * t0 + relaxation * t, 0.0f, 1.0f);
    };

    for (size_t i = 1; i < control_points.size() - 1; ++i) {
      const auto fl = face_strip[i];
      const auto fr = face_strip[i + 1];
      const auto e = surface.common_edge(fl, fr);
      edge_weights[i] = relax(control_points[i - 1], control_points[i + 1],
                              surface.position(e[0]), surface.position(e[1]),
                              edge_weights[i]);
    }

    if (closed()) {
      const auto fl = face_strip.front();
      const auto fr = face_strip[1];
      const auto e = surface.common_edge(fl, fr);
      edge_weights[0] = relax(control_points[control_points.size() - 2],
                              control_points[1], surface.position(e[0]),
                              surface.position(e[1]), edge_weights[0]);
    }

    // for (size_t i = 1; i < face_strip.size() - 2; i += 2) {
    //   const auto fll = face_strip[i - 1];
    //   const auto fl = face_strip[i];
    //   const auto fr = face_strip[i + 1];
    //   const auto frr = face_strip[i + 2];

    //   const auto pe = surface.common_edge(fll, fl);
    //   const auto l = surface.position(pe, edge_weights[i - 1]);

    //   const auto qe = surface.common_edge(fr, frr);
    //   const auto r = surface.position(qe, edge_weights[i + 1]);

    //   const auto e = surface.common_edge(fl, fr);

    //   const auto p = r - surface.position(e[0]);
    //   const auto q = l - surface.position(e[0]);
    //   const auto v = surface.position(e[1]) - surface.position(e[0]);
    //   const auto vl = length(v);
    //   const auto ivl = 1 / vl;
    //   const auto vn = ivl * v;

    //   const auto py = dot(p, vn);
    //   const auto qy = dot(q, vn);
    //   const auto px = -length(p - py * vn);
    //   const auto qx = length(q - qy * vn);

    //   const auto t = (py * qx - qy * px) / (qx - px) * ivl;
    //   const auto relaxation = 0.5f;
    //   edge_weights[i] = std::clamp(
    //       (1 - relaxation) * edge_weights[i] + relaxation * t, 0.0f, 1.0f);
    // }

    // if (closed()) {
    // }

    // for (size_t i = 2; i < face_strip.size() - 2; i += 2) {
    //   const auto fll = face_strip[i - 1];
    //   const auto fl = face_strip[i];
    //   const auto fr = face_strip[i + 1];
    //   const auto frr = face_strip[i + 2];

    //   const auto pe = surface.common_edge(fll, fl);
    //   const auto l = surface.position(pe, edge_weights[i - 1]);

    //   const auto qe = surface.common_edge(fr, frr);
    //   const auto r = surface.position(qe, edge_weights[i + 1]);

    //   const auto e = surface.common_edge(fl, fr);

    //   const auto p = r - surface.position(e[0]);
    //   const auto q = l - surface.position(e[0]);
    //   const auto v = surface.position(e[1]) - surface.position(e[0]);
    //   const auto vl = length(v);
    //   const auto ivl = 1 / vl;
    //   const auto vn = ivl * v;

    //   const auto py = dot(p, vn);
    //   const auto qy = dot(q, vn);
    //   const auto px = -length(p - py * vn);
    //   const auto qx = length(q - qy * vn);

    //   const auto t = (py * qx - qy * px) / (qx - px) * ivl;
    //   const auto relaxation = 0.5f;
    //   edge_weights[i] = std::clamp(
    //       (1 - relaxation) * edge_weights[i] + relaxation * t, 0.0f, 1.0f);
    // }
  }

  vector<uint32> face_strip;
  vector<float32> edge_weights;
  vector<vec3> control_points;
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
