#include <nanoreflex/ray_tracer.hpp>

namespace nanoreflex {

auto intersection(const ray& r, const triangle& f) noexcept
    -> ray_triangle_intersection {
  const auto edge1 = f[1] - f[0];
  const auto edge2 = f[2] - f[0];
  const auto p = cross(r.direction, edge2);
  const auto determinant = dot(edge1, p);
  if (0.0f == determinant) return {};
  const auto inverse_determinant = 1.0f / determinant;
  const auto s = r.origin - f[0];
  float u = dot(s, p) * inverse_determinant;
  const auto q = cross(s, edge1);
  float v = dot(r.direction, q) * inverse_determinant;
  float t = dot(edge2, q) * inverse_determinant;
  return {u, v, t};
}

auto intersection(const ray& r, const basic_scene& scene) noexcept
    -> ray_scene_intersection {
  ray_scene_intersection result{};
  result.t = infinity;
  for (size_t i = 0; i < scene.faces.size(); ++i) {
    const auto& v = scene.vertices;
    const auto& f = scene.faces[i];
    if (const auto p = intersection(
            r, {v[f[0]].position, v[f[1]].position, v[f[2]].position})) {
      if (p.t >= result.t) continue;
      static_cast<ray_triangle_intersection&>(result) = p;
      result.f = i;
    }
  }
  return result;
}

}  // namespace nanoreflex
