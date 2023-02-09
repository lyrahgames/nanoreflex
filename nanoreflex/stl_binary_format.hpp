#pragma once
#include <nanoreflex/utility.hpp>

namespace nanoreflex {

struct stl_binary_format {
  using header = array<uint8, 80>;
  using size_type = uint32;
  using attribute_byte_count_type = uint16;

  struct alignas(1) triangle {
    vec3 normal;
    vec3 vertex[3];
  };

  // This type is meant to represent the file information
  // of a binary STL file in the filesystem.
  // Hence, no factory function is used to construct it from a file.
  //
  stl_binary_format(const filesystem::path& path);

  vector<triangle> triangles{};
};

}  // namespace nanoreflex
