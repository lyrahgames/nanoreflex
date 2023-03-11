#pragma once
#include <nanoreflex/opengl/opengl.hpp>
#include <nanoreflex/stl_surface.hpp>
#include <nanoreflex/utility.hpp>

template <>
struct std::hash<glm::vec3> {
  constexpr auto operator()(const glm::vec3& v) const noexcept -> size_t {
    return (size_t(bit_cast<uint32_t>(v.x)) << 11) ^
           (size_t(bit_cast<uint32_t>(v.y)) << 5) ^  //
           size_t(bit_cast<uint32_t>(v.z));
  }
};

namespace nanoreflex::v2 {

struct polyhedral_surface {
  using size_type = uint32;

  using real = float32;
  static constexpr uint32 invalid = -1;

  struct vertex {
    vec3 position;
    vec3 normal;
  };
  using vertex_id = uint32;

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

  struct face : array<vertex_id, 3> {};
  using face_id = uint32;

  vector<vertex> vertices{};
  vector<face> faces{};

  void generate_topological_vertices();
  void generate_edges();
  void generate_face_neighbors();
  void generate_connection_groups();

  bool oriented() const noexcept;
  bool has_boundary() const noexcept;
  bool consistent() const noexcept;

  vector<vertex_id> topological_vertices{};
  unordered_map<edge, edge::info, edge::hasher> edges{};
  vector<array<uint32, 3>> face_neighbors{};

  // Face map
  using group_id = face_id;  // Every face could be part
  vector<group_id> connection_groups{};
  group_id connection_group_count = 0;
};

auto polyhedral_surface_from(const stl_surface& data) -> polyhedral_surface;

auto polyhedral_surface_from(const filesystem::path& path)
    -> polyhedral_surface;

struct scene : polyhedral_surface {
  auto host() noexcept -> polyhedral_surface& { return *this; }
  auto host() const noexcept -> const polyhedral_surface& { return *this; }

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

}  // namespace nanoreflex::v2
