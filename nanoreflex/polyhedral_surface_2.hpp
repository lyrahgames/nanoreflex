#pragma once
#include <nanoreflex/aabb.hpp>
#include <nanoreflex/discrete_quotient_map.hpp>
#include <nanoreflex/opengl/opengl.hpp>
#include <nanoreflex/stl_surface.hpp>
#include <nanoreflex/utility.hpp>

namespace nanoreflex {

struct polyhedral_surface {
  using size_type = uint32;

  using real = float32;
  static constexpr uint32 invalid = -1;

  struct vertex {
    vec3 position;
    vec3 normal;
  };
  using vertex_id = uint32;

  template <typename type>
  struct vertex_map : vector<type> {
    using base = vector<type>;
    using base::base;
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

  struct face : array<vertex_id, 3> {};
  using face_id = uint32;

  template <typename type>
  struct face_map : vector<type> {
    using base = vector<type>;
    using base::base;
  };

  vector<vertex> vertices{};
  vector<face> faces{};

  void generate_topological_vertices();
  void generate_edges();
  void generate_face_neighbors();

  bool oriented() const noexcept;
  bool has_boundary() const noexcept;
  bool consistent() const noexcept;

  auto position(vertex_id vid) const noexcept -> vec3;
  auto normal(vertex_id vid) const noexcept -> vec3;
  auto position(edge e, real t) const noexcept -> vec3;
  auto position(face_id fid, real u, real v) const noexcept -> vec3;

  auto shortest_face_path(uint32 src, uint32 dst) const -> vector<uint32>;
  auto common_edge(uint32 fid1, uint32 fid2) const -> edge;
  auto location(uint32 fid1, uint32 fid2) const -> uint32;

  vector<vertex_id> topological_vertices{};
  unordered_map<edge, edge::info, edge::hasher> edges{};
  vector<array<uint32, 3>> face_neighbors{};

  vector<vertex_id> vertex_offset{};

  using component_id = face_id;
  discrete_quotient_map<face_id, component_id> face_component_map{};

  void generate_face_component_map();
  auto component_count() const noexcept {
    return face_component_map.image_size();
  }
  auto component(face_id fid) const noexcept { return face_component_map(fid); }
  auto component_face_ids(component_id component) const noexcept {
    return face_component_map[component];
  }
  auto component_faces(component_id component) const noexcept {
    return component_face_ids(component) |
           views::transform([&](auto fid) { return faces[fid]; });
  }
};

auto polyhedral_surface_from(const stl_surface& data) -> polyhedral_surface;

auto polyhedral_surface_from(const filesystem::path& path)
    -> polyhedral_surface;

/// Constructor Extension for AABB
/// Get the bounding box around a polyhedral surface.
///
auto aabb_from(const polyhedral_surface& surface) noexcept -> aabb3;

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

}  // namespace nanoreflex
