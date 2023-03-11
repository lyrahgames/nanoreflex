#include <nanoreflex/polyhedral_surface_2.hpp>
//
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

namespace nanoreflex::v2 {

constexpr auto polyhedral_surface_from(const stl_surface& data)
    -> polyhedral_surface {
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
