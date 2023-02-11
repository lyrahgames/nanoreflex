#pragma once
#include <nanoreflex/aabb.hpp>
#include <nanoreflex/stl_binary_format.hpp>
#include <nanoreflex/utility.hpp>

namespace nanoreflex {

///
///
struct polyhedral_surface {
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

  struct face : array<uint32, 3> {};

  using face_id = uint32;

  /// Factory function to generate an instance of
  /// 'polyhedral_surface' from already loaded binary STL file.
  ///
  static auto from(const stl_binary_format& data) -> polyhedral_surface;

  /// Factory function to generate an instance of 'polyhedral_surface'
  /// from any kind of file format for surface meshes.
  ///
  static auto from(const filesystem::path& path) -> polyhedral_surface;

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

  auto position(vertex_id vid) const noexcept -> vec3;
  auto normal(vertex_id vid) const noexcept -> vec3;
  auto position(edge e, real t) const noexcept -> vec3;
  auto position(face_id fid, real u, real v) const noexcept -> vec3;

  auto shortest_face_path(uint32 src, uint32 dst) const -> vector<uint32>;
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

/// Constructor Extension for AABB
/// Get the bounding box around a polyhedral surface.
///
auto aabb_from(const polyhedral_surface& surface) noexcept -> aabb3;

}  // namespace nanoreflex
