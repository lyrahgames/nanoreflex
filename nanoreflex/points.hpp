#pragma once
#include <nanoreflex/opengl/opengl.hpp>

namespace nanoreflex {

struct points {
  void setup() noexcept {
    device_handle.bind();
    device_vertices.bind();

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), nullptr);
  }

  void update() noexcept { device_vertices.allocate_and_initialize(vertices); }

  void render() const noexcept {
    device_handle.bind();
    glDrawArrays(GL_POINTS, 0, vertices.size());
  }

  vector<vec3> vertices{};
  opengl::vertex_array device_handle{};
  opengl::vertex_buffer device_vertices{};
};

}  // namespace nanoreflex
