#pragma once
#include <nanoreflex/opengl/opengl.hpp>

namespace nanoreflex {

struct polyhedral_surface {
  static constexpr uint32 invalid = -1;

  struct vertex {
    vec3 position;
    vec3 normal;
  };

  struct edge : array<uint32, 2> {
    struct info {
      bool oriented() const noexcept { return face[1] == invalid; }
      void add_face(uint32 f, uint16 l);
      uint32 face[2]{invalid, invalid};
      uint16 location[2];
    };

    struct hasher {
      auto operator()(const edge& e) const noexcept -> size_t {
        return (size_t(e[0]) << 7) ^ size_t(e[1]);
      }
    };
  };

  struct face : array<uint32, 3> {};

  void clear() noexcept;
  void generate_normals() noexcept;
  void generate_edges();
  void generate_vertex_neighbors() noexcept;
  void generate_face_neighbors() noexcept;
  bool oriented() const noexcept;
  bool has_boundary() const noexcept;

  bool consistent() const noexcept;

  void generate_cohomology_groups() noexcept;
  void orient() noexcept;

  auto shortest_face_path(uint32 src, uint32 dst) const -> vector<uint32>;
  auto position(uint32 fid, float u, float v) const -> vec3;
  auto common_edge(uint32 fid1, uint32 fid2) const -> edge;

  vector<vertex> vertices{};
  vector<face> faces{};

  unordered_map<edge, edge::info, edge::hasher> edges{};

  vector<size_t> vertex_neighbor_offset{};
  vector<uint32> vertex_neighbors{};
  vector<bool> is_boundary_vertex{};

  vector<array<uint32, 3>> face_neighbors{};

  vector<uint32> cohomology_groups{};
  size_t cohomology_group_count = 0;

  vector<bool> orientation{};
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

void transform(const stl_binary_format& stl_data, polyhedral_surface& mesh);

void load_from_file(czstring file_path, polyhedral_surface& mesh);

struct scene : polyhedral_surface {
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
    device_faces.bind();
    glDrawElements(GL_TRIANGLES, 3 * faces.size(), GL_UNSIGNED_INT, 0);
  }

  opengl::vertex_array device_handle{};
  opengl::vertex_buffer device_vertices{};
  opengl::element_buffer device_faces{};
};

}  // namespace nanoreflex
