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
      const auto e =
          surface.common_edge(face_strip[i] >> 2, face_strip[i + 1] >> 2);
      control_points[i] = surface.position(e, edge_weights[i]);
    }
    if (closed()) control_points.push_back(control_points.front());
  }

  bool remove_artifacts(uint32 f) {
    const auto fid = face_strip.back() >> 2;
    if (f == fid) return true;
    if (face_strip.size() >= 2) {
      const auto fid2 = face_strip[face_strip.size() - 2] >> 2;
      if (f == fid2) {
        face_strip.pop_back();
        edge_weights.pop_back();
        return true;
      }
    }
    return false;
  }

  void add_face(uint32 f, const polyhedral_surface& surface) {
    if (face_strip.empty()) {
      face_strip.push_back(f << 2);
      return;
    }

    if (remove_artifacts(f)) return;
    const auto fid = face_strip.back() >> 2;
    const auto path = surface.shortest_face_path(fid, f);

    if (!path.empty())
      face_strip.back() = (fid << 2) | surface.location(fid, path.front() >> 2);

    for (auto x : path) {
      if (remove_artifacts(x >> 2)) continue;
      face_strip.push_back(x);
      edge_weights.push_back(0.5f);
    }
  }

  void remove_closed_artifacts() {
    if (face_strip.size() < 4) return;
    if ((face_strip.front() >> 2) != (face_strip.back() >> 2)) return;
    int cut = 0;
    while (true) {
      const auto fid1 = face_strip[1 + cut] >> 2;
      const auto fid2 = face_strip[face_strip.size() - 2 - cut] >> 2;
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

  void close(const polyhedral_surface& surface) {
    if (face_strip.size() < 3) return;

    const auto fid1 = face_strip.front() >> 2;
    const auto fid2 = face_strip.back() >> 2;
    const auto path = surface.shortest_face_path(fid2, fid1);

    if (!path.empty())
      face_strip.back() =
          (fid2 << 2) | surface.location(fid2, path.front() >> 2);

    for (auto x : path) {
      if (remove_artifacts(x >> 2)) continue;
      face_strip.push_back(x);
      edge_weights.push_back(0.5f);
    }

    remove_closed_artifacts();
  }

  bool closed() const {
    return (face_strip.front() >> 2) == (face_strip.back() >> 2);
  }

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
      const auto relaxation = 1.0f;
      return std::clamp((1 - relaxation) * t0 + relaxation * t, 0.0f, 1.0f);
    };

    for (size_t i = 1; i < control_points.size() - 1; ++i) {
      const auto fl = face_strip[i] >> 2;
      const auto fr = face_strip[i + 1] >> 2;
      const auto e = surface.common_edge(fl, fr);
      edge_weights[i] = relax(control_points[i - 1], control_points[i + 1],
                              surface.position(e[0]), surface.position(e[1]),
                              edge_weights[i]);
    }

    if (closed()) {
      const auto fl = face_strip.front() >> 2;
      const auto fr = face_strip[1] >> 2;
      const auto e = surface.common_edge(fl, fr);
      edge_weights[0] = relax(control_points[control_points.size() - 2],
                              control_points[1], surface.position(e[0]),
                              surface.position(e[1]), edge_weights[0]);
    }
  }

  void reflect(const polyhedral_surface& surface) {
    if (face_strip.size() < 5) return;

    const auto first = 1;

    auto f = face_strip[first];
    auto fid = f >> 2;
    auto loc = f & 0b11;
    f = surface.face_neighbors[fid][loc];
    fid = f >> 2;
    loc = f & 0b11;

    auto last = first + 1;
    auto f2 = face_strip[last];
    auto fid2 = f2 >> 2;
    auto loc2 = f2 & 0b11;

    const auto step = (3 + loc2 - loc) % 3;
    bool change = false;

    do {
      f = surface.face_neighbors[fid2][loc2];
      fid = f >> 2;
      loc = f & 0b11;
      ++last;
      f2 = face_strip[last];
      fid2 = f2 >> 2;
      loc2 = f2 & 0b11;
      const auto s = (3 + loc2 - loc) % 3;
      change = (s != step);
    } while (!change);

    for (size_t i = 0; i < face_strip.size(); ++i) {
      const auto fid = face_strip[i] >> 2;
      const auto loc = face_strip[i] & 0b11;
      cout << "(" << fid << "," << loc << ") ";
    }
    cout << endl;

    cout << first << ", " << last << endl;
    for (size_t i = first; i <= last; ++i) {
      const auto fid = face_strip[i] >> 2;
      const auto loc = face_strip[i] & 0b11;
      cout << "(" << fid << "," << loc << ") ";
    }
    cout << endl;

    vector<uint32> old_face_strip = face_strip;
    clear();
    add_face(old_face_strip[0] >> 2, surface);
    const auto rstep = (step == 1) ? 2 : 1;
    const auto shift = (step == 1) ? -1 : 1;
    auto fend = old_face_strip[last] >> 2;
    f = old_face_strip[first];
    fid = f >> 2;
    loc = ((f & 0b11) + shift) % 3;
    cout << "(" << fid << "," << loc << ") ";
    add_face(fid, surface);
    do {
      f = surface.face_neighbors[fid][loc];
      fid = f >> 2;
      loc = ((f & 0b11) + rstep) % 3;
      cout << "(" << fid << "," << loc << ") ";
      add_face(fid, surface);
    } while (fid != fend);
    cout << endl;

    for (size_t i = last + 1; i < old_face_strip.size(); ++i)
      add_face(old_face_strip[i] >> 2, surface);
  }

  void print(const polyhedral_surface& surface) {
    auto fid = face_strip.front() >> 2;
    auto loc = face_strip.front() & 0b11;

    auto f = surface.face_neighbors[fid][loc];
    fid = f >> 2;
    loc = f & 0b11;

    for (size_t i = 1; i < face_strip.size(); ++i) {
      const auto f2 = face_strip[i];
      const auto fid2 = f2 >> 2;
      const auto loc2 = f2 & 0b11;

      const auto step = (3 + loc2 - loc) % 3;

      // cout << fid << ", " << loc << " " << fid2 << ", " << loc2 << " -> "
      //      << step << endl;

      // assert(fid == fid2);
      // cout << step << ", ";
      if (step == 1) cout << "right" << endl;
      if (step == 2) cout << "left" << endl;

      f = surface.face_neighbors[fid2][loc2];
      fid = f >> 2;
      loc = f & 0b11;
    }
    cout << endl;
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
    const auto e = surface.common_edge(curve.face_strip[i] >> 2,
                                       curve.face_strip[i + 1] >> 2);
    points[i] = surface.position(e, curve.edge_weights[i]);
  }
  if (curve.closed()) points.push_back(points.front());

  return points;
}

}  // namespace nanoreflex
