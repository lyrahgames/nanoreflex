#pragma once
#include <nanoreflex/polyhedral_surface_2.hpp>
#include <nanoreflex/scene.hpp>

namespace nanoreflex {

struct ray {
  auto operator()(float t) const noexcept { return origin + t * direction; }

  vec3 origin;
  vec3 direction;
};

struct triangle : array<vec3, 3> {};

struct ray_triangle_intersection {
  operator bool() const noexcept {
    return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0) && (t > 0.0f);
  }

  float u{};
  float v{};
  float t{};
};

auto intersection(const ray& r, const triangle& f) noexcept
    -> ray_triangle_intersection;

struct ray_polyhedral_surface_intersection : ray_triangle_intersection {
  // We overwrite the check,
  // because it the triangle will already have been checked.
  operator bool() const noexcept { return f != -1; }
  uint32_t f = -1;
};

auto intersection(const ray& r, const polyhedral_surface& scene) noexcept
    -> ray_polyhedral_surface_intersection;

auto intersection(const ray& r, const v2::polyhedral_surface& scene) noexcept
    -> ray_polyhedral_surface_intersection;

}  // namespace nanoreflex
