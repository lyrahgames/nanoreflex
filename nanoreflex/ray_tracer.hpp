#pragma once
#include <nanoreflex/utility.hpp>

namespace nanoreflex {

struct ray {
  vec3 origin;
  vec3 direction;
};

struct ray_triangle_intersection {
  operator bool() const noexcept {
    return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0) && (t > 0.0f);
  }

  float u{};
  float v{};
  float t{};
};

inline auto intersection(const ray& r, const array<vec3, 3>& triangle) noexcept
    -> ray_triangle_intersection {
  const auto edge1 = triangle[1] - triangle[0];
  const auto edge2 = triangle[2] - triangle[0];
  const auto p = cross(r.direction, edge2);
  const auto determinant = dot(edge1, p);
  if (0.0f == determinant) return {};
  const auto inverse_determinant = 1.0f / determinant;
  const auto s = r.origin - triangle[0];
  float u = dot(s, p) * inverse_determinant;
  const auto q = cross(s, edge1);
  float v = dot(r.direction, q) * inverse_determinant;
  float t = dot(edge2, q) * inverse_determinant;
  return {u, v, t};
}

}  // namespace nanoreflex
