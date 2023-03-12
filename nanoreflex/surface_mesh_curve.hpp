#pragma once
#include <nanoreflex/polyhedral_surface_2.hpp>

namespace nanoreflex {

struct surface_mesh_curve {
  vector<uint32> face_strip;
  vector<float32> edge_weights;
};

}  // namespace nanoreflex
