#pragma once
#include <nanoreflex/scene.hpp>

namespace nanoreflex {

struct ray {
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

struct ray_scene_intersection : ray_triangle_intersection {
  // We overwrite the check,
  // because it the triangle will already have been checked.
  operator bool() const noexcept { return f != -1; }
  uint32_t f = -1;
};

auto intersection(const ray& r, const basic_scene& scene) noexcept
    -> ray_scene_intersection;

}  // namespace nanoreflex
