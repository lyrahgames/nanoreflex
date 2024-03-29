#include <nanoreflex/polyhedral_surface.hpp>
#include <nanoreflex/surface_mesh_curve.hpp>

namespace nanoreflex {

bool polyhedral_surface::valid(const surface_mesh_curve& curve) const noexcept {
  if (curve.face_strip.size() != curve.edge_weights.size() + 1) return false;

  for (size_t i = 1; i < curve.face_strip.size(); ++i) {
    const auto f = curve.face_strip[i];
    const auto fid = f >> 2;
    const auto loc = f & 0b11;

    const auto pfid = curve.face_strip[i - 1] >> 2;
    if (pfid != (face_adjacencies[fid][loc] >> 2)) return false;
  }

  return true;
}

auto polyhedral_surface::points_from(const surface_mesh_curve& curve) const
    -> vector<vec3> {
  vector<vec3> points{};
  for (size_t i = 0; i < curve.size(); ++i) {
    const auto f1 = curve.face_strip[i];
    const auto f2 = curve.face_strip[i + 1];
    const auto fid = f2 >> 2;
    const auto loc = f2 & 0b11;
    points.push_back(position(edge{faces[fid][loc], faces[fid][(loc + 1) % 3]},
                              curve.edge_weights[i]));
  }
  return points;
}

auto polyhedral_surface::shortest_surface_mesh_curve(face_id src,
                                                     face_id dst) const
    -> surface_mesh_curve {
  const auto barycenter = [&](face_id fid) {
    const auto& f = faces[fid];
    return (vertices[f[0]].position + vertices[f[1]].position +
            vertices[f[2]].position) /
           3.0f;
  };
  const auto face_distance = [&](face_id i, face_id j) {
    return glm::distance(barycenter(i), barycenter(j));
  };

  vector<bool> visited(faces.size(), false);

  vector<float> distances(faces.size(), INFINITY);
  distances[src] = 0;

  vector<uint32> previous(faces.size());
  previous[src] = src;

  vector<uint32> queue{src};
  const auto order = [&](uint32 i, uint32 j) {
    return distances[i] > distances[j];
  };

  do {
    ranges::make_heap(queue, order);
    ranges::pop_heap(queue, order);
    const auto current = queue.back();
    queue.pop_back();

    // cout << "current = " << current << endl;

    visited[current] = true;

    const auto neighbor_faces = face_adjacencies[current];
    for (int i = 0; i < 3; ++i) {
      const auto n = neighbor_faces[i];
      if (n == invalid) continue;
      const auto neighbor = (n >> 2);
      const auto loc = n & 0b11;
      if (visited[neighbor]) continue;

      const auto d = face_distance(current, neighbor) + distances[current];
      if (d >= distances[neighbor]) continue;

      distances[neighbor] = d;
      // previous[neighbor] = (current << 2) | uint32(i);
      previous[neighbor] = loc;
      queue.push_back(neighbor);
    }
  } while (!queue.empty() && !visited[dst]);

  if (queue.empty()) return {};

  // Compute count and path.
  uint32 count = 0;
  // for (auto i = dst; i != src; i = (previous[i] >> 2)) ++count;
  for (auto i = dst; i != src; i = (face_adjacencies[i][previous[i]] >> 2))
    ++count;

  surface_mesh_curve curve;
  curve.face_strip.resize(count);
  curve.edge_weights.assign(count, 0.5f);

  // vector<uint32> path(count);
  // for (auto i = dst; i != src; i = (previous[i] >> 2))
  //   path[--count] = uint32(i << 2) | uint32(previous[i] & 0b11);
  for (auto i = dst; i != src;) {
    curve.face_strip[--count] = (i << 2) | previous[i];
    i = (face_adjacencies[i][previous[i]] >> 2);
  }
  return curve;
}

void polyhedral_surface::add_face(surface_mesh_curve& curve,
                                  face_id fid) const {
  if (curve.face_strip.empty()) {
    curve.face_strip.push_back(fid << 2);
    return;
  }

  const auto last = curve.face_strip.back() >> 2;
  if (fid == last) return;

  auto path = shortest_surface_mesh_curve(last, fid);

  for (size_t i = 0; i < path.size(); ++i) {
    if (curve.face_strip.size() > 1) {
      const auto last = curve.face_strip.back();
      const auto last_fid = last >> 2;
      const auto last_loc = last & 0b11;
      if (face_adjacencies[last_fid][last_loc] == path.face_strip[i]) {
        curve.face_strip.pop_back();
        curve.edge_weights.pop_back();
        continue;
      }
    }

    curve.face_strip.push_back(path.face_strip[i]);
    curve.edge_weights.push_back(path.edge_weights[i]);
  }
}

auto polyhedral_surface::critical_points_from(
    const surface_mesh_curve& curve) const -> vector<vec3> {
  vector<vec3> points{};

  auto f1 = curve.face_strip[1];
  auto fid1 = f1 >> 2;
  auto loc1 = f1 & 0b11;
  auto f2 = curve.face_strip[2];
  auto fid2 = f2 >> 2;
  auto loc2 = f2 & 0b11;
  f2 = face_adjacencies[fid2][loc2];
  fid2 = f2 >> 2;
  loc2 = f2 & 0b11;
  auto step = (3 + loc2 - loc1) % 3;
  assert(fid1 == fid2);
  assert(step != 0);

  for (size_t i = 3; i < curve.face_strip.size(); ++i) {
    f1 = curve.face_strip[i - 1];
    fid1 = f1 >> 2;
    loc1 = f1 & 0b11;
    f2 = curve.face_strip[i];
    fid2 = f2 >> 2;
    loc2 = f2 & 0b11;
    f2 = face_adjacencies[fid2][loc2];
    fid2 = f2 >> 2;
    loc2 = f2 & 0b11;
    const auto s = (3 + loc2 - loc1) % 3;
    assert(fid1 == fid2);
    assert(s != 0);

    if (step != s) {
      const auto inner = position(faces[fid2][(loc1 + 2 - step) % 3]);
      points.push_back(inner);
      step = s;
      cout << i - 1 << ", ";
    }
  }
  cout << curve.face_strip.size() - 1;
  cout << endl;

  return points;
}

void surface_mesh_curve::clear() {
  face_strip.clear();
  edge_weights.clear();
  control_points.clear();
}

bool surface_mesh_curve::remove_artifacts(uint32 f) {
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

void surface_mesh_curve::generate_control_points(
    const polyhedral_surface& surface) {
  // if (face_strip.size() < 2) return;

  // control_points.reserve(face_strip.size());
  // control_points.resize(face_strip.size() - 1);

  // for (size_t i = 0; i < face_strip.size() - 1; ++i) {
  //   const auto e =
  //       surface.common_edge(face_strip[i] >> 2, face_strip[i + 1] >> 2);
  //   control_points[i] = surface.position(e, edge_weights[i]);
  // }
  // if (closed()) control_points.push_back(control_points.front());

  control_points.clear();
  if (face_strip.empty()) return;
  control_points.reserve(face_strip.size() + 1);

  if (!closed())
    control_points.push_back(
        surface.position(face_strip.front() >> 2, 1 / 3.0f, 1 / 3.0f));
  else
    control_points.push_back({});

  for (size_t i = 0; i < face_strip.size() - 1; ++i) {
    const auto e =
        surface.common_edge(face_strip[i] >> 2, face_strip[i + 1] >> 2);
    control_points.push_back(surface.position(e, edge_weights[i]));
  }

  if (!closed())
    control_points.push_back(
        surface.position(face_strip.back() >> 2, 1 / 3.0f, 1 / 3.0f));
  else {
    control_points.front() = control_points.back();
    control_points.push_back(control_points[1]);
  }
}

void surface_mesh_curve::add_face(uint32 f, const polyhedral_surface& surface) {
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

void surface_mesh_curve::remove_closed_artifacts() {
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
  vector<uint32> new_face_strip(begin(face_strip) + cut, end(face_strip) - cut);
  face_strip.swap(new_face_strip);
  vector<float32> new_edge_weights(begin(edge_weights) + cut,
                                   end(edge_weights) - cut);
  edge_weights.swap(new_edge_weights);
}

void surface_mesh_curve::close(const polyhedral_surface& surface) {
  if (face_strip.size() < 3) return;

  const auto fid1 = face_strip.front() >> 2;
  const auto fid2 = face_strip.back() >> 2;
  const auto path = surface.shortest_face_path(fid2, fid1);

  if (!path.empty())
    face_strip.back() = (fid2 << 2) | surface.location(fid2, path.front() >> 2);

  for (auto x : path) {
    if (remove_artifacts(x >> 2)) continue;
    face_strip.push_back(x);
    edge_weights.push_back(0.5f);
  }

  remove_closed_artifacts();
}

bool surface_mesh_curve::closed() const {
  // return (face_strip.front() >> 2) == (face_strip.back() >> 2);
  return (face_strip.size() > 2) &&
         ((face_strip.front() >> 2) == (face_strip.back() >> 2));
}

void surface_mesh_curve::smooth(const polyhedral_surface& surface) {
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
    const auto relaxation = 0.5f;
    return std::clamp((1 - relaxation) * t0 + relaxation * t, 0.0f, 1.0f);
  };

  // for (size_t i = 1; i < control_points.size() - 1; ++i) {
  //   const auto fl = face_strip[i - 1] >> 2;
  //   const auto fr = face_strip[i] >> 2;
  //   const auto e = surface.common_edge(fl, fr);
  //   edge_weights[i - 1] = relax(control_points[i - 1], control_points[i + 1],
  //                               surface.position(e[0]),
  //                               surface.position(e[1]), edge_weights[i - 1]);
  // }

  vector<float> new_edge_weights(edge_weights.size());
  for (size_t i = 0; i < edge_weights.size(); ++i) {
    const auto fl = face_strip[i] >> 2;
    const auto fr = face_strip[i + 1] >> 2;
    const auto e = surface.common_edge(fl, fr);
    new_edge_weights[i] =
        relax(control_points[i], control_points[i + 2], surface.position(e[0]),
              surface.position(e[1]), edge_weights[i]);
  }
  edge_weights.swap(new_edge_weights);

  // if (closed()) {
  //   const auto fl = face_strip.front() >> 2;
  //   const auto fr = face_strip[1] >> 2;
  //   const auto e = surface.common_edge(fl, fr);
  //   edge_weights[0] = relax(control_points[control_points.size() - 2],
  //                           control_points[1], surface.position(e[0]),
  //                           surface.position(e[1]), edge_weights[0]);
  // }
}

auto surface_mesh_curve::reflect(size_t first,
                                 const polyhedral_surface& surface,
                                 surface_mesh_curve& result) {
  // auto f = face_strip[first];
  auto f = result.face_strip.back();
  auto fid = f >> 2;
  auto loc = f & 0b11;
  f = surface.face_adjacencies[fid][loc];
  fid = f >> 2;
  loc = f & 0b11;

  auto f2 = face_strip[first];
  auto fid2 = f2 >> 2;
  auto loc2 = f2 & 0b11;

  const auto step = (3 + loc2 - loc) % 3;
  const auto inner =
      surface.position(surface.faces[fid2][(loc + 2 - step) % 3]);
  auto outer1 = result.control_points[result.control_points.size() - 2];
  auto outer2 = surface.position(surface.faces[fid2][(loc + step - 1) % 3]);

  auto curve_angle =
      acos(dot(normalize(outer1 - inner), normalize(outer2 - inner)));
  outer1 = outer2;

  size_t i = first + 1;
  for (; i < face_strip.size() - 1; ++i) {
    f = surface.face_adjacencies[fid2][loc2];
    fid = f >> 2;
    loc = f & 0b11;
    f2 = face_strip[i];
    fid2 = f2 >> 2;
    loc2 = f2 & 0b11;
    const auto s = (3 + loc2 - loc) % 3;
    if (s != step) break;

    outer2 = surface.position(surface.faces[fid2][(loc + step - 1) % 3]);
    curve_angle +=
        acos(dot(normalize(outer1 - inner), normalize(outer2 - inner)));
    outer1 = outer2;
  }

  size_t index = i;
  // cout << "index = " << index << endl;
  // if (step == 1)
  //   cout << "right" << endl;
  // else
  //   cout << "left" << endl;

  outer2 = control_points[index + 1];
  curve_angle +=
      acos(dot(normalize(outer1 - inner), normalize(outer2 - inner)));

  cout << "curve angle = " << curve_angle * 180 / pi << "°" << endl;

  if (curve_angle <= pi) {
    for (size_t i = first; i < index; ++i) {
      result.face_strip.push_back(face_strip[i]);
      result.edge_weights.push_back(edge_weights[i - 1]);
    }
  } else {
    for (size_t i = first; i <= index; ++i) {
      const auto f = face_strip[i];
      const auto fid = f >> 2;
      const auto loc = f & 0b11;
      cout << "(" << fid << "," << loc << ") ";
    }
    cout << endl;

    const auto rstep = (step == 1) ? 2 : 1;
    const auto shift = (step == 1) ? -1 : 1;

    const auto flast = face_strip[index] >> 2;
    f = result.face_strip.back();
    result.face_strip.pop_back();
    result.edge_weights.pop_back();
    fid = f >> 2;
    const auto ffirst = fid;
    loc = ((f & 0b11) + 3 + shift) % 3;
    i = 0;
    while (fid != flast) {
      cout << "(" << fid << "," << loc << ") ";
      result.face_strip.push_back((fid << 2) | loc);
      result.edge_weights.push_back(0.5f);

      f = surface.face_adjacencies[fid][loc];
      if (f == polyhedral_surface::invalid) {
        cout << "invalid path" << endl;
        break;
      }
      fid = f >> 2;
      loc = ((f & 0b11) + 3 + rstep) % 3;
      ++i;
      if (fid == ffirst) {
        cout << "endless loop detected" << endl;
        break;
      }
    }
    cout << endl;
  }

  return index;
}

auto surface_mesh_curve::reflect(const polyhedral_surface& surface)
    -> surface_mesh_curve {
  // if (face_strip.size() < 5) return;

  cout << "Reflection" << endl;
  for (size_t i = 0; i < face_strip.size(); ++i) {
    const auto f = face_strip[i];
    const auto fid = f >> 2;
    const auto loc = f & 0b11;
    cout << "(" << fid << "," << loc << ")";
  }
  cout << endl << endl;

  surface_mesh_curve result{};

  size_t index = 1;
  result.face_strip.push_back(face_strip.front());
  result.generate_control_points(surface);
  while (index < face_strip.size() - 1) {
    index = reflect(index, surface, result);
    result.generate_control_points(surface);
    cout << "index = " << index << endl;
  }
  result.face_strip.push_back(face_strip.back());
  result.edge_weights.push_back(edge_weights.back());

  // auto f = face_strip[0];
  // auto fid = f >> 2;
  // auto loc = f & 0b11;
  // f = surface.face_adjacencies[fid][loc];
  // fid = f >> 2;
  // loc = f & 0b11;

  // auto f2 = face_strip[1];
  // auto fid2 = f2 >> 2;
  // auto loc2 = f2 & 0b11;

  // const auto step = (3 + loc2 - loc) % 3;
  // const auto inner =
  //     surface.position(surface.faces[fid2][(loc + 2 - step) % 3]);
  // auto outer1 = control_points[0];
  // auto outer2 = surface.position(surface.faces[fid2][(loc + step - 1) % 3]);

  // auto curve_angle =
  //     acos(dot(normalize(outer1 - inner), normalize(outer2 - inner)));
  // outer1 = outer2;

  // bool change = false;

  // size_t i = 2;
  // for (; i < face_strip.size() - 1; ++i) {
  //   f = surface.face_adjacencies[fid2][loc2];
  //   fid = f >> 2;
  //   loc = f & 0b11;
  //   f2 = face_strip[i];
  //   fid2 = f2 >> 2;
  //   loc2 = f2 & 0b11;
  //   const auto s = (3 + loc2 - loc) % 3;
  //   if (s != step) break;

  //   outer2 = surface.position(surface.faces[fid2][(loc + step - 1) % 3]);
  //   curve_angle +=
  //       acos(dot(normalize(outer1 - inner), normalize(outer2 - inner)));
  //   outer1 = outer2;
  // }
  // size_t index = i;
  // cout << "index = " << index << endl;
  // if (step == 1)
  //   cout << "right" << endl;
  // else
  //   cout << "left" << endl;

  // outer2 = control_points[index + 1];
  // curve_angle +=
  //     acos(dot(normalize(outer1 - inner), normalize(outer2 - inner)));

  // cout << "curve angle = " << curve_angle * 180 / pi << "°" << endl;

  // if (curve_angle <= pi) {
  //   for (size_t i = 0; i < index; ++i) {
  //     result.face_strip.push_back(face_strip[i]);
  //     result.edge_weights.push_back(0.5f);
  //   }
  // } else {
  //   for (size_t i = 0; i <= index; ++i) {
  //     const auto f = face_strip[i];
  //     const auto fid = f >> 2;
  //     const auto loc = f & 0b11;
  //     cout << "(" << fid << "," << loc << ") ";
  //   }
  //   cout << endl;

  //   const auto rstep = (step == 1) ? 2 : 1;
  //   const auto shift = (step == 1) ? -1 : 1;

  //   const auto flast = face_strip[index] >> 2;
  //   f = face_strip[0];
  //   fid = f >> 2;
  //   const auto ffirst = fid;
  //   loc = ((f & 0b11) + 3 + shift) % 3;
  //   i = 0;
  //   while (fid != flast) {
  //     cout << "(" << fid << "," << loc << ") ";
  //     result.face_strip.push_back((fid << 2) | loc);
  //     if (result.face_strip.size() > 0) result.edge_weights.push_back(0.5f);

  //     f = surface.face_adjacencies[fid][loc];
  //     if (f == polyhedral_surface::invalid) {
  //       cout << "invalid path" << endl;
  //       break;
  //     }
  //     fid = f >> 2;
  //     loc = ((f & 0b11) + 3 + rstep) % 3;
  //     ++i;
  //     if (fid == ffirst) {
  //       cout << "endless loop detected" << endl;
  //       break;
  //     }
  //   }
  //   cout << endl;
  // }

  // // result.face_strip.push_back(face_strip[0]);
  // for (size_t i = 0; i < face_strip.size(); ++i) {
  //   const auto f = face_strip[i];
  //   const auto fid = f >> 2;
  //   const auto loc = f & 0b11;
  //   cout << "(" << fid << "," << loc << ") ";
  //   result.face_strip.push_back(face_strip[i]);
  //   result.edge_weights.push_back(edge_weights[i - 1]);
  // }
  // cout << endl;

  return result;
}

void surface_mesh_curve::print(const polyhedral_surface& surface) {
  auto fid = face_strip.front() >> 2;
  auto loc = face_strip.front() & 0b11;

  auto f = surface.face_adjacencies[fid][loc];
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

    f = surface.face_adjacencies[fid2][loc2];
    fid = f >> 2;
    loc = f & 0b11;
  }
  cout << endl;
}

}  // namespace nanoreflex
