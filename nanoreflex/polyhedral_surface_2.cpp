#include <nanoreflex/polyhedral_surface_2.hpp>
//
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

namespace nanoreflex::v2 {

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

void polyhedral_surface::generate_topological_vertices() {
  unordered_map<vec3, vertex_id> indices{};
  indices.reserve(vertices.size());

  topological_vertices.resize(vertices.size());
  vertex_id id = 0;

  for (vertex_id i = 0; i < vertices.size(); ++i) {
    const auto position = vertices[i].position;
    const auto it = indices.find(position);
    if (it == end(indices)) {
      topological_vertices[i] = id;
      indices.emplace(position, id++);
      continue;
    }
    topological_vertices[i] = it->second;
  }
}

void polyhedral_surface::generate_edges() {
  edges.clear();
  for (size_t i = 0; i < faces.size(); ++i) {
    const auto& f = faces[i];
    edges[edge{topological_vertices[f[0]], topological_vertices[f[1]]}]
        .add_face(i, 0);
    edges[edge{topological_vertices[f[1]], topological_vertices[f[2]]}]
        .add_face(i, 1);
    edges[edge{topological_vertices[f[2]], topological_vertices[f[0]]}]
        .add_face(i, 2);
  }
}

void polyhedral_surface::generate_face_neighbors() {
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

void polyhedral_surface::generate_connection_groups() {
  cout << "Generate connection_groups" << endl;
  // connection_groups.assign(faces.size(), invalid);
  connection_groups.resize(faces.size());
  for (size_t i = 0; i < faces.size(); ++i) connection_groups[i] = invalid;

  vector<face_id> face_stack{};
  group_id group = 0;

  for (face_id fid = 0; fid < faces.size(); ++fid) {
    if (connection_groups[fid] != invalid) continue;
    face_stack.push_back(fid);
    while (!face_stack.empty()) {
      const auto f = face_stack.back();
      face_stack.pop_back();
      connection_groups[f] = group;
      const auto& face = faces[f];
      for (int i = 0; i < 3; ++i) {
        const auto nid = face_neighbors[f][i];
        if (nid == invalid) continue;
        if (connection_groups[nid] != invalid) continue;
        face_stack.push_back(nid);
      }
    }
    ++group;
  }
  connection_group_count = group;
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

auto polyhedral_surface_from(const stl_surface& data) -> polyhedral_surface {
  using size_type = polyhedral_surface::size_type;
  static_assert(same_as<size_type, stl_surface::size_type>);

  polyhedral_surface surface{};
  surface.vertices.resize(data.triangles.size() * 3);
  surface.faces.resize(data.triangles.size());

  for (size_type i = 0; i < data.triangles.size(); ++i) {
    for (size_type j = 0; j < 3; ++j)
      surface.vertices[3 * i + j] = {
          .position = data.triangles[i].vertex[j],
          .normal = data.triangles[i].normal,
      };
    surface.faces[i] = {3 * i + 0, 3 * i + 1, 3 * i + 2};
  }

  return surface;
}

auto polyhedral_surface_from(const filesystem::path& path)
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
    return polyhedral_surface_from(stl_surface(path));

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

}  // namespace nanoreflex::v2
