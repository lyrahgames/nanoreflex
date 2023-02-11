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
    opengl::shader_program shader;
    filesystem::file_time_type last_change;
    filesystem::file_time_type last_access;
  };

  void add_shader(const filesystem::path& path) {
    shaders.emplace(path, shader_data{.shader = opengl::shader_from_file(path),
                                      .last_change = last_changed(path),
                                      .last_access = clock::now()});
  }

  void update_shader(const filesystem::path& path, shader_data& data) {
    const auto time = last_changed(path);
    if (time <= data.last_access) return;
    cout << "Shader " << proximate(path) << " has changed. Reload triggered."
         << endl;
    // Here, the order for exception throws is important.
    data.last_access = clock::now();
    // Shader compilation could throw errors.
    data.shader = opengl::shader_from_file(path);
    data.last_change = time;
  }

  void load_shader(const filesystem::path& path) {
    const auto p = canonical(path);
    const auto it = shaders.find(p);
    if (it == end(shaders))
      add_shader(p);
    else
      update_shader(it->first, it->second);
  }

  void add_name(const filesystem::path& path, const string& name) {
    names[name] = shaders.find(canonical(path));
  }

  void reload(auto&& function) {
    for (auto& [path, data] : shaders) {
      try {
        update_shader(path, data);
        function(data.shader);
      } catch (const runtime_error& e) {
        cerr << e.what() << endl;
      }
    }
  }

  auto apply(auto&& function) -> shader_manager& {
    for (auto& [path, data] : shaders) function(data.shader);
    return *this;
  }

  auto set(czstring name, auto&& value) -> shader_manager& {
    for (auto& [path, data] : shaders)
      data.shader.bind().set(name, forward<decltype(value)>(value));
    return *this;
  }

  auto try_set(czstring name, auto&& value) noexcept -> shader_manager& {
    for (auto& [path, data] : shaders)
      data.shader.bind().try_set(name, forward<decltype(value)>(value));
    return *this;
  }

  unordered_map<filesystem::path, shader_data> shaders{};
  unordered_map<string, decltype(shaders.begin())> names{};
};

}  // namespace nanoreflex
