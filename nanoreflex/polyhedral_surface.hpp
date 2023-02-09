#pragma once
#include <nanoreflex/stl_binary_format.hpp>
#include <nanoreflex/utility.hpp>

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

  static auto from(const stl_binary_format& data) -> polyhedral_surface;
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

}  // namespace nanoreflex
