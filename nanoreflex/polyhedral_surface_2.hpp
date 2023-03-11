#pragma once
#include <nanoreflex/stl_surface.hpp>
#include <nanoreflex/utility.hpp>

namespace nanoreflex::v2 {

struct polyhedral_surface_base {
  using real = float32;
  static constexpr uint32 invalid = -1;

  struct vertex {
    vec3 position;
    vec3 normal;
  };
  using vertex_id = uint32;

  struct face : array<vertex_id, 3> {};
  using face_id = uint32;

  vector<vertex> vertices{};
  vector<face> faces{};
};

constexpr auto polyhedral_surface_from(const stl_surface& data)
    -> polyhedral_surface_base;

}  // namespace nanoreflex::v2
