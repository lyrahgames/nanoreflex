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

  void clear() noexcept;

  vector<vertex> vertices{};
  vector<face> faces{};

  void generate_edges();

  bool oriented() const noexcept;

  static constexpr auto pair_hasher = [](const auto& x) {
    return (x.first << 7) ^ x.second;
  };

  unordered_map<pair<size_t, size_t>, int, decltype(pair_hasher)> edges{};
};

struct stl_binary_format {
  using header = array<uint8_t, 80>;
  using size_type = uint32_t;
  using attribute_byte_count_type = uint16_t;

  struct alignas(1) triangle {
    vec3 normal;
    vec3 vertex[3];
  };

  static_assert(offsetof(triangle, normal) == 0);
  static_assert(offsetof(triangle, vertex[0]) == 12);
  static_assert(offsetof(triangle, vertex[1]) == 24);
  static_assert(offsetof(triangle, vertex[2]) == 36);
  static_assert(sizeof(triangle) == 48);
  static_assert(alignof(triangle) == 4);

  stl_binary_format() = default;
  stl_binary_format(czstring file_path);

  vector<triangle> triangles{};
};

void transform(const stl_binary_format& stl_data, basic_scene& mesh);

void load_from_file(czstring file_path, basic_scene& mesh);

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
