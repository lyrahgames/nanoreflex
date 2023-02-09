#include <nanoreflex/stl_binary_format.hpp>

namespace nanoreflex {

stl_binary_format::stl_binary_format(const filesystem::path& path) {
  // Provide some static assertions that make sure the loading works properly.
  static_assert(offsetof(triangle, normal) == 0);
  static_assert(offsetof(triangle, vertex[0]) == 12);
  static_assert(offsetof(triangle, vertex[1]) == 24);
  static_assert(offsetof(triangle, vertex[2]) == 36);
  static_assert(sizeof(triangle) == 48);
  static_assert(alignof(triangle) == 4);

  fstream file{path, ios::in | ios::binary};
  if (!file.is_open())
    throw runtime_error("Failed to open STL file from path '"s + path.string() +
                        "'.");

  // We will ignore the header.
  // It has no specific use to us.
  file.ignore(sizeof(header));

  // Read number of triangles.
  size_type size;
  file.read((char*)&size, sizeof(size));

  triangles.resize(size);

  // Due to padding and alignment issues concerning 'float32' and 'uint16',
  // we cannot read everything at once.
  // Instead, we use a simple loop for every triangle.
  for (auto& t : triangles) {
    file.read((char*)&t, sizeof(triangle));
    // Ignore the attribute byte count.
    // There should not be any information anyway.
    file.ignore(sizeof(attribute_byte_count_type));
  }
}

}  // namespace nanoreflex
