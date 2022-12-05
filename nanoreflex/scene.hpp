#pragma once
#include <nanoreflex/opengl/opengl.hpp>
#include <nanoreflex/utility.hpp>

namespace nanoreflex {

struct basic_scene {
  struct vertex {
    vec3 position;
    vec3 normal;
  };

  struct face : array<uint32, 3> {};

  vector<vertex> vertices{};
  vector<face> faces{};
};

struct scene : basic_scene {
  void setup() noexcept {
    device_handle.bind();
    device_vertices.bind();
    device_faces.bind();

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex),
                          (void*)offsetof(vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex),
                          (void*)offsetof(vertex, normal));
  }

  void update() noexcept {
    device_vertices.allocate_and_initialize(vertices);
    device_faces.allocate_and_initialize(faces);
  }

  void render() const noexcept {
    device_handle.bind();
    glDrawElements(GL_TRIANGLES, 3 * faces.size(), GL_UNSIGNED_INT, 0);
  }

  opengl::vertex_array device_handle{};
  opengl::vertex_buffer device_vertices{};
  opengl::element_buffer device_faces{};
};

}  // namespace nanoreflex
