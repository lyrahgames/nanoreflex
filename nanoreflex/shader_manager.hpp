#pragma once
#include <nanoreflex/opengl/opengl.hpp>

namespace nanoreflex {

struct shader_manager {
  using time_point = filesystem::file_time_type;
  using clock = time_point::clock;

  static auto last_changed(const filesystem::path& path) -> time_point {
    auto time = last_write_time(path);
    for (const auto& entry : filesystem::recursive_directory_iterator(path))
      time = std::max(time, entry.last_write_time());
    return time;
  }

  struct shader_data {
    shader_program shader;
    filesystem::file_time_type last_change;
    filesystem::file_time_type last_access;
  };

  void load_shader(const filesystem::path& path) {
    bool success = true;
    try {
      const auto shader = shader_from_file(path);
    } catch (runtime_error& e) {
      success = false;
      cerr << "ERROR:\n" << e.what() << endl;
    }

    const auto last_change = last_changed(path);
    const auto last_access = clock::now();

    const auto p = canonical(path);
    const auto it = shaders.find(p);
    if (it == end(shaders)) {
      shaders.emplace(p, {shader, last_change, last_access});
      return;
    }
    auto& data = it->second;
    data.last_access = last_access;
  }

  unordered_map<filesystem::path, shader_data> shaders{};
};

}  // namespace nanoreflex
