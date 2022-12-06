#include <nanoreflex/scene.hpp>
//
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

namespace nanoreflex {

void basic_scene::clear() noexcept {
  vertices.clear();
  faces.clear();
}

void basic_scene::generate_normals() noexcept {
  for (auto& v : vertices) v.normal = {};
  for (const auto& f : faces) {
    const auto p = vertices[f[1]].position - vertices[f[0]].position;
    const auto q = vertices[f[2]].position - vertices[f[0]].position;
    const auto n = cross(p, q) / dot(p, p) / dot(q, q);
    vertices[f[0]].normal += n;
    vertices[f[1]].normal += n;
    vertices[f[2]].normal += n;
  }
  for (auto& v : vertices) v.normal = normalize(v.normal);
}

void basic_scene::generate_edges() {
  edges.clear();
  for (size_t i = 0; i < faces.size(); ++i) {
    const auto& f = faces[i];
    ++edges[pair{f[0], f[1]}];
    ++edges[pair{f[1], f[2]}];
    ++edges[pair{f[2], f[0]}];
  }
}

void basic_scene::generate_vertex_neighbors() noexcept {
  // Generate undirected edges.
  unordered_set<pair<size_t, size_t>, decltype(pair_hasher)> undirected_edges{};
  for (const auto& [e, _] : edges)
    undirected_edges.insert(
        pair{std::min(e.first, e.second), std::max(e.first, e.second)});

  // Count neighbors of each vertex.
  vertex_neighbor_offset.resize(vertices.size() + 1);
  vertex_neighbor_offset[0] = 0;
  for (const auto& e : undirected_edges) {
    ++vertex_neighbor_offset[e.first + 1];
    ++vertex_neighbor_offset[e.second + 1];
  }
  // Compute cumulative sum to generate neighbor offsets.
  for (size_t i = 2; i <= vertices.size(); ++i)
    vertex_neighbor_offset[i] += vertex_neighbor_offset[i - 1];

  // Assign neighbors to the pre-allocated neighbor vector.
  vector<size_t> neighbor_count(vertices.size(), 0);
  vertex_neighbors.resize(vertex_neighbor_offset.back());
  for (const auto& e : undirected_edges) {
    vertex_neighbors[vertex_neighbor_offset[e.first] +
                     neighbor_count[e.first]++] = e.second;
    vertex_neighbors[vertex_neighbor_offset[e.second] +
                     neighbor_count[e.second]++] = e.first;
  }
}

void basic_scene::orient() noexcept {
  for (auto& f : faces) {
    if (edges.contains(pair{f[1], f[0]}) ||  //
        edges.contains(pair{f[2], f[1]}) ||  //
        edges.contains(pair{f[0], f[2]}))
      continue;
    --edges[pair{f[0], f[1]}];
    --edges[pair{f[1], f[2]}];
    --edges[pair{f[2], f[0]}];
    swap(f[1], f[2]);
    ++edges[pair{f[0], f[1]}];
    ++edges[pair{f[1], f[2]}];
    ++edges[pair{f[2], f[0]}];
  }
}

bool basic_scene::oriented() const noexcept {
  for (auto& [e, insertions] : edges)
    if (insertions != 1) return false;
  return true;
}

bool basic_scene::has_boundary() const noexcept {
  for (auto& [e, insertions] : edges)
    if (!edges.contains(pair{e.second, e.first}) && (insertions == 1))
      return true;
  return false;
}

auto basic_scene::intersection(const ray& r) const noexcept
    -> ray_intersection {
  ray_intersection result{};
  result.t = infinity;
  for (size_t i = 0; i < faces.size(); ++i) {
    const auto& v = vertices;
    const auto& f = faces[i];
    if (const auto intersection = nanoreflex::intersection(
            r, {v[f[0]].position, v[f[1]].position, v[f[2]].position})) {
      if (intersection.t >= result.t) continue;
      static_cast<ray_triangle_intersection&>(result) = intersection;
      result.f = i;
    }
  }
  return result;
}

stl_binary_format::stl_binary_format(czstring file_path) {
  fstream file{file_path, ios::in | ios::binary};
  if (!file.is_open()) throw runtime_error("Failed to open given STL file.");

  // We will ignore the header.
  // It has no specific use to us.
  file.ignore(sizeof(header));

  // Read number of triangles.
  size_type size;
  file.read((char*)&size, sizeof(size));

  // Due to padding and alignment issues,
  // we cannot read everything at once.
  // Hence, we use a simple loop for every triangle.
  triangles.resize(size);
  for (auto& t : triangles) {
    file.read((char*)&t, sizeof(triangle));
    // Ignore the attribute byte count.
    // There should not be any information anyway.
    file.ignore(sizeof(attribute_byte_count_type));
  }
}

void transform(const stl_binary_format& stl_data, basic_scene& mesh) {
  unordered_map<vec3, size_t, decltype([](const auto& v) -> size_t {
                  return (bit_cast<uint32_t>(v.x) << 11) ^
                         (bit_cast<uint32_t>(v.y) << 5) ^
                         bit_cast<uint32_t>(v.z);
                })>
      position_index{};
  position_index.reserve(stl_data.triangles.size());

  mesh.clear();
  mesh.faces.reserve(stl_data.triangles.size());
  mesh.vertices.reserve(stl_data.triangles.size() / 2);

  for (size_t i = 0; i < stl_data.triangles.size(); ++i) {
    basic_scene::face f{};
    const auto& normal = stl_data.triangles[i].normal;
    const auto& v = stl_data.triangles[i].vertex;
    for (size_t j = 0; j < 3; ++j) {
      const auto it = position_index.find(v[j]);
      if (it == end(position_index)) {
        const int index = mesh.vertices.size();
        f[j] = index;
        position_index.emplace(v[j], index);
        mesh.vertices.push_back({v[j]});
        continue;
      }

      const auto index = it->second;
      f[j] = index;
    }
    // Remove degenerate triangles.
    if ((f[0] == f[1]) || (f[1] == f[2]) || (f[2] == f[0])) continue;
    mesh.faces.push_back(f);
  }
  mesh.generate_normals();
}

void load_from_file(czstring file_path, basic_scene& mesh) {
  const auto path = filesystem::path(file_path);
  if (!exists(path))
    throw runtime_error("Failed to load surface mesh from file '"s +
                        path.string() + "'. The file does not exist.");

  // Use a custom loader for STL files.
  if (path.extension().string() == ".stl" ||
      path.extension().string() == ".STL") {
    transform(stl_binary_format{path.c_str()}, mesh);
    return;
  }

  // For all other file formats, assimp will do the trick.
  Assimp::Importer importer{};
  importer.SetPropertyInteger(
      AI_CONFIG_PP_RVC_FLAGS,
      aiComponent_NORMALS | aiComponent_TANGENTS_AND_BITANGENTS |
          aiComponent_COLORS | aiComponent_TEXCOORDS | aiComponent_BONEWEIGHTS |
          aiComponent_ANIMATIONS | aiComponent_TEXTURES | aiComponent_LIGHTS |
          aiComponent_CAMERAS /*| aiComponent_MESHES*/ | aiComponent_MATERIALS);

  const auto post_processing =
      aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals |
      aiProcess_JoinIdenticalVertices | aiProcess_RemoveComponent |
      aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph |
      aiProcess_FindDegenerates | aiProcess_DropNormals;

  const auto raw = importer.ReadFile(file_path, post_processing);

  if (!raw || raw->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !raw->mRootNode)
    throw runtime_error(string("Failed to load surface from file '") +
                        file_path + "'.");

  if (raw->mNumMeshes > 1)
    throw runtime_error("Failed to transform surface to a single mesh.");

  mesh.vertices.resize(raw->mMeshes[0]->mNumVertices);
  for (size_t i = 0; i < mesh.vertices.size(); ++i) {
    mesh.vertices[i].position = {raw->mMeshes[0]->mVertices[i].x,  //
                                 raw->mMeshes[0]->mVertices[i].y,  //
                                 raw->mMeshes[0]->mVertices[i].z};
    mesh.vertices[i].normal = {raw->mMeshes[0]->mNormals[i].x,  //
                               raw->mMeshes[0]->mNormals[i].y,  //
                               raw->mMeshes[0]->mNormals[i].z};
  }

  mesh.faces.resize(raw->mMeshes[0]->mNumFaces);
  for (size_t i = 0; i < mesh.faces.size(); ++i) {
    assert(raw->mMeshes[0]->mFaces[i].mNumIndices == 3);
    for (size_t j = 0; j < 3; j++)
      mesh.faces[i][j] = raw->mMeshes[0]->mFaces[i].mIndices[j];
  }
}

}  // namespace nanoreflex
