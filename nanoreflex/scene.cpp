#include <nanoreflex/scene.hpp>
//
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

namespace nanoreflex {

void basic_scene::edge::info::add_face(uint32 f, uint16 l) {
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

void basic_scene::clear() noexcept {
  vertices.clear();
  faces.clear();
}

void basic_scene::generate_normals() noexcept {
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

void basic_scene::generate_edges() {
  edges.clear();
  for (size_t i = 0; i < faces.size(); ++i) {
    const auto& f = faces[i];
    edges[edge{f[0], f[1]}].add_face(i, 0);
    edges[edge{f[1], f[2]}].add_face(i, 1);
    edges[edge{f[2], f[0]}].add_face(i, 2);
  }
}

void basic_scene::generate_vertex_neighbors() noexcept {
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

void basic_scene::generate_face_neighbors() noexcept {
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

bool basic_scene::oriented() const noexcept {
  for (auto& [e, info] : edges)
    if (!info.oriented()) return false;
  return true;
}

bool basic_scene::has_boundary() const noexcept {
  for (auto& [e, info] : edges)
    if (!edges.contains(edge{e[1], e[0]}) && info.oriented()) return true;
  return false;
}

bool basic_scene::consistent() const noexcept {
  for (auto& [e, info] : edges)
    if (!info.oriented() && edges.contains(edge{e[1], e[0]})) return false;
  return true;
}

void basic_scene::generate_cohomology_groups() noexcept {
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

void basic_scene::orient() noexcept {
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
