#pragma once
#include <nanoreflex/polyhedral_surface.hpp>

namespace nanoreflex {

struct surface_mesh_curve {
  void clear();
  bool remove_artifacts(uint32 f);
  void generate_control_points(const polyhedral_surface& surface);
  void add_face(uint32 f, const polyhedral_surface& surface);
  void remove_closed_artifacts();
  void close(const polyhedral_surface& surface);
  bool closed() const;
  void smooth(const polyhedral_surface& surface);
  auto reflect(size_t first,
               const polyhedral_surface& surface,
               surface_mesh_curve& result);
  auto reflect(const polyhedral_surface& surface) -> surface_mesh_curve;
  void print(const polyhedral_surface& surface);

  vector<uint32> face_strip;
  vector<float32> edge_weights;
  vector<vec3> control_points;
};

}  // namespace nanoreflex
