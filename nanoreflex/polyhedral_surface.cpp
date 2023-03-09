#include <nanoreflex/polyhedral_surface.hpp>
//
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

namespace nanoreflex {

void polyhedral_surface::edge::info::add_face(uint32 f, uint16 l) {
  if (face[0] == invalid) {
    face[0] = f;
    location[0] = l;
  } else if (face[1] == invalid) {
    face[1] = f;
    location[1] = l;
  } else
    throw runtime_error(
        "Failed to add face to edge. Additional face would violate "
        "requirements for a two-dimensional manifold.");
}

void polyhedral_surface::clear() noexcept {
  vertices.clear();
  faces.clear();
}

void polyhedral_surface::generate_normals() noexcept {
  for (auto& v : vertices) v.normal = {};
  for (const auto& f : faces) {
    for (int i = 0; i < 3; ++i) {
      const auto p =
          vertices[f[(i + 1) % 3]].position - vertices[f[i]].position;
      const auto q =
          vertices[f[(i + 2) % 3]].position - vertices[f[i]].position;

      const auto lp = length(p);
      const auto lq = length(q);
      const auto np = p / lp;
      const auto nq = q / lq;
      const auto n = cross(np, nq) / lp / lq;
      vertices[f[i]].normal += n;
    }
  }
  for (auto& v : vertices) v.normal = normalize(v.normal);
}

void polyhedral_surface::generate_edges() {
  edges.clear();
  for (size_t i = 0; i < faces.size(); ++i) {
    const auto& f = faces[i];
    edges[edge{f[0], f[1]}].add_face(i, 0);
    edges[edge{f[1], f[2]}].add_face(i, 1);
    edges[edge{f[2], f[0]}].add_face(i, 2);
  }
}

void polyhedral_surface::generate_vertex_neighbors() noexcept {
  // Generate undirected edges.
  unordered_set<edge, edge::hasher> undirected_edges{};
  for (const auto& [e, _] : edges)
    undirected_edges.insert(edge{std::min(e[0], e[1]), std::max(e[0], e[1])});
  // Count neighbors of each vertex.
  vertex_neighbor_offset.clear();
  vertex_neighbor_offset.resize(vertices.size() + 1);
  vertex_neighbor_offset[0] = 0;
  for (const auto& e : undirected_edges) {
    ++vertex_neighbor_offset[e[0] + 1];
    ++vertex_neighbor_offset[e[1] + 1];
  }
  // Compute cumulative sum to generate neighbor offsets.
  for (size_t i = 2; i <= vertices.size(); ++i)
    vertex_neighbor_offset[i] += vertex_neighbor_offset[i - 1];

  // Assign neighbors to the pre-allocated neighbor vector.
  vector<size_t> neighbor_count(vertices.size(), 0);
  // cout << "size = " << vertex_neighbor_offset.back() << endl;
  vertex_neighbors.resize(vertex_neighbor_offset.back());
  for (const auto& e : undirected_edges) {
    vertex_neighbors[vertex_neighbor_offset[e[0]] + neighbor_count[e[0]]++] =
        e[1];
    vertex_neighbors[vertex_neighbor_offset[e[1]] + neighbor_count[e[1]]++] =
        e[0];
  }
}

void polyhedral_surface::generate_face_neighbors() noexcept {
  face_neighbors.resize(faces.size());
  for (const auto& [e, info] : edges) {
    if (info.oriented()) {
      const auto it = edges.find(edge{e[1], e[0]});
      if (it == end(edges))
        face_neighbors[info.face[0]][info.location[0]] = invalid;
      else {
        const auto& [e2, info2] = *it;
        face_neighbors[info.face[0]][info.location[0]] = info2.face[0];
        // face_neighbors[info2.face[0]][info2.location[0]] = info.face[0];
      }
    } else {
      face_neighbors[info.face[0]][info.location[0]] = info.face[1];
      face_neighbors[info.face[1]][info.location[1]] = info.face[0];
    }
  }
}

bool polyhedral_surface::oriented() const noexcept {
  for (auto& [e, info] : edges)
    if (!info.oriented()) return false;
  return true;
}

bool polyhedral_surface::has_boundary() const noexcept {
  for (auto& [e, info] : edges)
    if (!edges.contains(edge{e[1], e[0]}) && info.oriented()) return true;
  return false;
}

bool polyhedral_surface::consistent() const noexcept {
  for (auto& [e, info] : edges)
    if (!info.oriented() && edges.contains(edge{e[1], e[0]})) return false;
  return true;
}

void polyhedral_surface::generate_cohomology_groups() noexcept {
  cohomology_groups.resize(faces.size());
  orientation.resize(faces.size());
  for (size_t i = 0; i < faces.size(); ++i) cohomology_groups[i] = invalid;

  vector<uint32_t> face_stack{};
  uint32 group = 0;

  for (size_t fid = 0; fid < faces.size(); ++fid) {
    if (cohomology_groups[fid] != invalid) continue;
    face_stack.push_back(fid);
    orientation[fid] = false;
    while (!face_stack.empty()) {
      const auto f = face_stack.back();
      face_stack.pop_back();
      cohomology_groups[f] = group;
      const auto& face = faces[f];
      for (int i = 0; i < 3; ++i) {
        const auto nid = face_neighbors[f][i];
        if (nid == invalid) continue;
        if (cohomology_groups[nid] != invalid) continue;
        face_stack.push_back(nid);

        if (edges[edge{face[i], face[(i + 1) % 3]}].oriented())
          orientation[nid] = orientation[f];
        else
          orientation[nid] = !orientation[f];
      }
    }
    ++group;
  }

  cohomology_group_count = group;
}

void polyhedral_surface::orient() noexcept {
  for (size_t fid = 0; fid < faces.size(); ++fid) {
    if (!orientation[fid]) continue;
    auto& face = faces[fid];
    swap(face[1], face[2]);
  }
  generate_normals();
  generate_edges();
  generate_vertex_neighbors();
  generate_face_neighbors();
  generate_cohomology_groups();
}

auto polyhedral_surface::shortest_face_path(uint32 src, uint32 dst) const
    -> vector<uint32> {
  const auto barycenter = [&](uint32 fid) {
    const auto& f = faces[fid];
    return (vertices[f[0]].position + vertices[f[1]].position +
            vertices[f[2]].position) /
           3.0f;
  };
  const auto face_distance = [&](uint32 i, uint32 j) {
    return glm::distance(barycenter(i), barycenter(j));
  };

  vector<bool> visited(faces.size(), false);

  vector<float> distances(faces.size(), INFINITY);
  distances[src] = 0;

  vector<uint32> previous(faces.size());
  previous[src] = src;

  vector<uint32> queue{src};
  const auto order = [&](uint32 i, uint32 j) {
    return distances[i] > distances[j];
  };

  do {
    ranges::make_heap(queue, order);
    ranges::pop_heap(queue, order);
    const auto current = queue.back();
    queue.pop_back();

    // cout << "current = " << current << endl;

    visited[current] = true;

    const auto neighbor_faces = face_neighbors[current];
    for (int i = 0; i < 3; ++i) {
      const auto neighbor = neighbor_faces[i];
      if (visited[neighbor]) continue;

      const auto d = face_distance(current, neighbor) + distances[current];
      if (d >= distances[neighbor]) continue;

      distances[neighbor] = d;
      previous[neighbor] = current;
      queue.push_back(neighbor);
    }
  } while (!queue.empty() && !visited[dst]);

  if (queue.empty()) return {};

  // Compute count and path.
  uint32 count = 0;
  for (auto i = dst; i != src; i = previous[i]) ++count;
  vector<uint32> path(count);
  for (auto i = dst; i != src; i = previous[i]) path[--count] = i;
  return path;
}

auto polyhedral_surface::position(vertex_id vid) const noexcept -> vec3 {
  return vertices[vid].position;
}

auto polyhedral_surface::normal(vertex_id vid) const noexcept -> vec3 {
  return vertices[vid].normal;
}

auto polyhedral_surface::position(edge e, real t) const noexcept -> vec3 {
  return (real(1) - t) * position(e[0]) + t * position(e[1]);
}

auto polyhedral_surface::position(face_id fid, real u, real v) const noexcept
    -> vec3 {
  const auto& f = faces[fid];
  const auto w = real(1) - u - v;
  return position(f[0]) * w + position(f[1]) * u + position(f[2]) * v;
}

auto polyhedral_surface::common_edge(uint32 fid1, uint32 fid2) const -> edge {
  const auto& f1 = faces[fid1];
  const auto& f2 = faces[fid2];

  if (face_neighbors[fid2][0] == fid1) return {f2[0], f2[1]};
  if (face_neighbors[fid2][1] == fid1) return {f2[1], f2[2]};
  if (face_neighbors[fid2][2] == fid1) return {f2[2], f2[0]};

  throw runtime_error("Triangles have no common edge.");
}

auto polyhedral_surface::from(const stl_binary_format& data)
    -> polyhedral_surface {
  // The hash map is used to efficiently recognize identical vertices.
  unordered_map<
      vec3, size_t,
      // For hashing 'vec3' values, a custom hash function is provided.
      decltype([](const auto& v) -> size_t {
        return (bit_cast<uint32>(v.x) << 11) ^
               (bit_cast<uint32>(v.y) << 5) ^  //
               bit_cast<uint32>(v.z);
      })>
      indices{};
  indices.reserve(data.triangles.size());

  // Prepare the storage for the actual surface.
  polyhedral_surface surface{};
  surface.faces.reserve(data.triangles.size());
  surface.vertices.reserve(data.triangles.size() / 2);

  //
  for (size_t i = 0; i < data.triangles.size(); ++i) {
    polyhedral_surface::face f{};
    const auto& normal = data.triangles[i].normal;
    const auto& v = data.triangles[i].vertex;
    for (size_t j = 0; j < 3; ++j) {
      const auto it = indices.find(v[j]);
      if (it == end(indices)) {
        const int index = surface.vertices.size();
        f[j] = index;
        indices.emplace(v[j], index);
        surface.vertices.push_back({v[j]});
        continue;
      }

      const auto index = it->second;
      f[j] = index;
    }
    // Remove degenerate triangles by not adding them.
    if ((f[0] == f[1]) || (f[1] == f[2]) || (f[2] == f[0])) continue;
    surface.faces.push_back(f);
  }
  surface.generate_normals();
  return surface;
}

auto polyhedral_surface::from(const stl_format_surface& data)
    -> polyhedral_surface {
  // The hash map is used to efficiently recognize identical vertices.
  unordered_map<
      vec3, size_t,
      // For hashing 'vec3' values, a custom hash function is provided.
      decltype([](const auto& v) -> size_t {
        return (bit_cast<uint32>(v.x) << 11) ^
               (bit_cast<uint32>(v.y) << 5) ^  //
               bit_cast<uint32>(v.z);
      })>
      indices{};
  indices.reserve(data.triangles.size());

  // Prepare the storage for the actual surface.
  polyhedral_surface surface{};
  surface.faces.reserve(data.triangles.size());
  surface.vertices.reserve(data.triangles.size() / 2);

  //
  for (size_t i = 0; i < data.triangles.size(); ++i) {
    polyhedral_surface::face f{};
    const auto& normal = data.triangles[i].normal;
    const auto& v = data.triangles[i].vertex;
    for (size_t j = 0; j < 3; ++j) {
      const auto it = indices.find(v[j]);
      if (it == end(indices)) {
        const int index = surface.vertices.size();
        f[j] = index;
        indices.emplace(v[j], index);
        surface.vertices.push_back({v[j]});
        continue;
      }

      const auto index = it->second;
      f[j] = index;
    }
    // Remove degenerate triangles by not adding them.
    if ((f[0] == f[1]) || (f[1] == f[2]) || (f[2] == f[0])) continue;
    surface.faces.push_back(f);
  }
  surface.generate_normals();
  return surface;
}

auto polyhedral_surface::from(const filesystem::path& path)
    -> polyhedral_surface {
  // Generate functor for prefixed error messages.
  const auto throw_error = [&](czstring str) {
    throw runtime_error("Failed to load 'polyhedral_surface' from path '"s +
                        path.string() + "'. " + str);
  };

  if (!exists(path)) throw_error("The path does not exist.");

  // Use a custom loader for STL files.
  if (path.extension().string() == ".stl" ||
      path.extension().string() == ".STL")
    // return from(stl_binary_format(path));
    return from(stl_format_surface(path));

  // For all other file formats, assimp will do the trick.
  Assimp::Importer importer{};
  // Assimp only needs to generate a continuously connected surface.
  // So, all other information can be stripped from vertices.
  importer.SetPropertyInteger(
      AI_CONFIG_PP_RVC_FLAGS,
      aiComponent_NORMALS | aiComponent_TANGENTS_AND_BITANGENTS |
          aiComponent_COLORS | aiComponent_TEXCOORDS | aiComponent_BONEWEIGHTS |
          aiComponent_ANIMATIONS | aiComponent_TEXTURES | aiComponent_LIGHTS |
          aiComponent_CAMERAS /*| aiComponent_MESHES*/ | aiComponent_MATERIALS);
  // After the stripping and loading,
  // Assimp shall generate a single mesh object.
  // The surface is only allowed to consist of triangles
  // and all identical points need to be connected.
  const auto post_processing =
      aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals |
      aiProcess_JoinIdenticalVertices | aiProcess_RemoveComponent |
      aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph |
      aiProcess_FindDegenerates | aiProcess_DropNormals;
  //
  const auto scene = importer.ReadFile(path.c_str(), post_processing);

  // Check whether Assimp could load the file at all.
  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    throw_error("Assimp could not process the file.");
  //
  if (scene->mNumMeshes > 1)
    throw_error("Assimp could not transform the data into a single mesh.");

  // Transform the loaded mesh data from
  // Assimp's internal structure to a polyhedral surface.
  polyhedral_surface surface{};
  // Get all vertices.
  surface.vertices.resize(scene->mMeshes[0]->mNumVertices);
  for (size_t i = 0; i < surface.vertices.size(); ++i) {
    surface.vertices[i].position = {scene->mMeshes[0]->mVertices[i].x,  //
                                    scene->mMeshes[0]->mVertices[i].y,  //
                                    scene->mMeshes[0]->mVertices[i].z};
    surface.vertices[i].normal = {scene->mMeshes[0]->mNormals[i].x,  //
                                  scene->mMeshes[0]->mNormals[i].y,  //
                                  scene->mMeshes[0]->mNormals[i].z};
  }
  // Get all the faces.
  surface.faces.resize(scene->mMeshes[0]->mNumFaces);
  for (size_t i = 0; i < surface.faces.size(); ++i) {
    // All faces need to be triangles.
    assert(scene->mMeshes[0]->mFaces[i].mNumIndices == 3);
    surface.faces[i][0] = scene->mMeshes[0]->mFaces[i].mIndices[0];
    surface.faces[i][1] = scene->mMeshes[0]->mFaces[i].mIndices[1];
    surface.faces[i][2] = scene->mMeshes[0]->mFaces[i].mIndices[2];
  }
  return surface;
}

auto aabb_from(const polyhedral_surface& surface) noexcept -> aabb3 {
  return aabb_from(surface.vertices |
                   views::transform([](const auto& x) { return x.position; }));
}

}  // namespace nanoreflex
