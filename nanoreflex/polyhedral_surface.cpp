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

void polyhedral_surface::generate_topological_vertex_map() {
  const auto indices = views::iota(vertex_id(0), vertices.size());
  topological_vertex_map = {
      indices,
      [&](auto vid1, auto vid2) { return position(vid1) == position(vid2); },
      [&](auto vid) {
        const auto v = position(vid);
        return (size_t(bit_cast<uint32_t>(v.x)) << 11) ^
               (size_t(bit_cast<uint32_t>(v.y)) << 5) ^
               size_t(bit_cast<uint32_t>(v.z));
      }};
  assert(topological_vertex_map.valid());
}

void polyhedral_surface::generate_edges() {
  edges.clear();
  for (size_t i = 0; i < faces.size(); ++i) {
    const auto& f = faces[i];
    edges[edge{topological_vertex_map(f[0]), topological_vertex_map(f[1])}]
        .add_face(i, 0);
    edges[edge{topological_vertex_map(f[1]), topological_vertex_map(f[2])}]
        .add_face(i, 1);
    edges[edge{topological_vertex_map(f[2]), topological_vertex_map(f[0])}]
        .add_face(i, 2);
  }
}

void polyhedral_surface::generate_face_adjacencies() {
  face_adjacencies.resize(faces.size());
  for (const auto& [e, info] : edges) {
    if (info.oriented()) {
      const auto it = edges.find(edge{e[1], e[0]});
      if (it == end(edges))
        face_adjacencies[info.face[0]][info.location[0]] = invalid;
      else {
        const auto& [e2, info2] = *it;
        face_adjacencies[info.face[0]][info.location[0]] =
            uint32(info2.face[0] << 2) | uint32(info2.location[0]);
      }
    } else {
      face_adjacencies[info.face[0]][info.location[0]] =
          uint32(info.face[1] << 2) | uint32(info.location[1]);
      face_adjacencies[info.face[1]][info.location[1]] =
          uint32(info.face[0] << 2) | uint32(info.location[0]);
    }
  }
}

void polyhedral_surface::generate_face_component_map() {
  vector<component_id> face_component(faces.size(), invalid);

  vector<face_id> face_stack{};
  component_id component = 0;

  for (face_id fid = 0; fid < faces.size(); ++fid) {
    if (face_component[fid] != invalid) continue;
    face_stack.push_back(fid);
    while (!face_stack.empty()) {
      const auto f = face_stack.back();
      face_stack.pop_back();
      face_component[f] = component;
      const auto& face = faces[f];
      for (int i = 0; i < 3; ++i) {
        const auto n = face_adjacencies[f][i];
        if (n == invalid) continue;
        const auto nid = (n >> 2);
        if (face_component[nid] != invalid) continue;
        face_stack.push_back(nid);
      }
    }
    ++component;
  }
  // component_count = component;

  face_component_map = {move(face_component), component};
  assert(face_component_map.valid());
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

    const auto neighbor_faces = face_adjacencies[current];
    for (int i = 0; i < 3; ++i) {
      const auto n = neighbor_faces[i];
      if (n == invalid) continue;
      const auto neighbor = (n >> 2);
      if (visited[neighbor]) continue;

      const auto d = face_distance(current, neighbor) + distances[current];
      if (d >= distances[neighbor]) continue;

      distances[neighbor] = d;
      previous[neighbor] = (current << 2) | uint32(i);
      queue.push_back(neighbor);
    }
  } while (!queue.empty() && !visited[dst]);

  if (queue.empty()) return {};

  // Compute count and path.
  uint32 count = 0;
  for (auto i = dst; i != src; i = (previous[i] >> 2)) ++count;
  vector<uint32> path(count);
  // for (auto i = dst; i != src; i = (previous[i] >> 2))
  //   path[--count] = uint32(i << 2) | uint32(previous[i] & 0b11);
  uint32 l = 0;
  for (auto i = dst; i != src;) {
    path[--count] = (i << 2) | l;
    l = previous[i] & 0b11;
    i = previous[i] >> 2;
  }
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

  if ((face_adjacencies[fid2][0] >> 2) == fid1) return {f2[0], f2[1]};
  if ((face_adjacencies[fid2][1] >> 2) == fid1) return {f2[1], f2[2]};
  if ((face_adjacencies[fid2][2] >> 2) == fid1) return {f2[2], f2[0]};

  throw runtime_error("Triangles "s + to_string(fid1) + " and " +
                      to_string(fid2) + " have no common edge.");
}

auto polyhedral_surface::location(uint32 fid1, uint32 fid2) const -> uint32 {
  const auto& f1 = faces[fid1];
  const auto& f2 = faces[fid2];

  if ((face_adjacencies[fid1][0] >> 2) == fid2) return 0;
  if ((face_adjacencies[fid1][1] >> 2) == fid2) return 1;
  if ((face_adjacencies[fid1][2] >> 2) == fid2) return 2;

  throw runtime_error("Triangles are not adjacent to each other.");
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
  //
  const auto throw_error = [&](czstring str) {
    throw runtime_error("Failed to load 'polyhedral_surface' from path '"s +
                        path.string() + "'. " + str);
  };

  if (!exists(path)) throw_error("The path does not exist.");

  // Use a custom loader for STL files.
  //
  if (path.extension().string() == ".stl" ||
      path.extension().string() == ".STL")
    return polyhedral_surface_from(stl_surface(path));

  // For all other file formats, assimp will do the trick.
  //
  Assimp::Importer importer{};

  // Assimp only needs to generate a continuously connected surface.
  // So, a lot of information can be stripped from vertices.
  //
  importer.SetPropertyInteger(
      AI_CONFIG_PP_RVC_FLAGS,
      /*aiComponent_NORMALS |*/ aiComponent_TANGENTS_AND_BITANGENTS |
          aiComponent_COLORS |
          /*aiComponent_TEXCOORDS |*/ aiComponent_BONEWEIGHTS |
          aiComponent_ANIMATIONS | aiComponent_TEXTURES | aiComponent_LIGHTS |
          aiComponent_CAMERAS /*| aiComponent_MESHES*/ | aiComponent_MATERIALS);

  // After the stripping and loading,
  // certain post processing steps are mandatory.
  //
  const auto post_processing =
      aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals |
      aiProcess_JoinIdenticalVertices | aiProcess_RemoveComponent |
      /*aiProcess_OptimizeMeshes |*/ /*aiProcess_OptimizeGraph |*/
      aiProcess_FindDegenerates /*| aiProcess_DropNormals*/;

  // Now, let Assimp actually load a surface scene from the given file.
  //
  const auto scene = importer.ReadFile(path.c_str(), post_processing);

  // Check whether Assimp could load the file at all.
  //
  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    throw_error("Assimp could not process the file.");

  // Now, transform the loaded mesh data from
  // Assimp's internal structure to a polyhedral surface.
  //
  polyhedral_surface surface{};

  // First, get the total number of vertices
  // and faces and for all meshes.
  //
  size_t vertex_count = 0;
  size_t face_count = 0;
  for (size_t i = 0; i < scene->mNumMeshes; ++i) {
    vertex_count += scene->mMeshes[i]->mNumVertices;
    face_count += scene->mMeshes[i]->mNumFaces;
  }
  //
  surface.vertices.resize(vertex_count);
  surface.faces.resize(face_count);

  // Iterate over all meshes and get the vertex and face information.
  // All meshes will be linearly stored in one polyhedral surface.
  //
  uint32 vertex_offset = 0;
  uint32 face_offset = 0;
  for (size_t mid = 0; mid < scene->mNumMeshes; ++mid) {
    // Vertices of the Mesh
    //
    for (size_t vid = 0; vid < scene->mMeshes[mid]->mNumVertices; ++vid) {
      surface.vertices[vid + vertex_offset] = {
          .position = {scene->mMeshes[mid]->mVertices[vid].x,  //
                       scene->mMeshes[mid]->mVertices[vid].y,  //
                       scene->mMeshes[mid]->mVertices[vid].z},
          .normal = {scene->mMeshes[mid]->mNormals[vid].x,  //
                     scene->mMeshes[mid]->mNormals[vid].y,  //
                     scene->mMeshes[mid]->mNormals[vid].z}};
    }

    // Faces of the Mesh
    //
    for (size_t fid = 0; fid < scene->mMeshes[mid]->mNumFaces; ++fid) {
      // All faces need to be triangles.
      // So, use a simple triangulation of polygons.
      const auto corners = scene->mMeshes[mid]->mFaces[fid].mNumIndices;
      for (size_t k = 2; k < corners; ++k) {
        surface.faces[face_offset + fid] = {
            scene->mMeshes[mid]->mFaces[fid].mIndices[0] + vertex_offset,  //
            scene->mMeshes[mid]->mFaces[fid].mIndices[k - 1] +
                vertex_offset,  //
            scene->mMeshes[mid]->mFaces[fid].mIndices[k] + vertex_offset};
      }
    }

    // Update offsets to not overwrite previously written meshes.
    //
    vertex_offset += scene->mMeshes[mid]->mNumVertices;
    face_offset += scene->mMeshes[mid]->mNumFaces;
  }

  return surface;
}

auto aabb_from(const polyhedral_surface& surface) noexcept -> aabb3 {
  return nanoreflex::aabb_from(
      surface.vertices |
      views::transform([](const auto& x) { return x.position; }));
}

}  // namespace nanoreflex
