#pragma once
#include <nanoreflex/camera.hpp>
#include <nanoreflex/opengl/opengl.hpp>
#include <nanoreflex/scene.hpp>
#include <nanoreflex/utility.hpp>

namespace nanoreflex {

class viewer_context {
 public:
  viewer_context();

 protected:
  sf::Window window{};
};

class viewer : viewer_context {
 public:
  viewer();
  void resize();
  void resize(int width, int height);
  void process_events();
  void update();
  void update_view();
  void render();
  void run();

  void turn(const vec2& angle);
  void shift(const vec2& pixels);
  void zoom(float scale);
  void look_at(float x, float y);

  void set_z_as_up();
  void set_y_as_up();

  void load_scene(czstring file_path);
  void fit_view();

  void load_shader(czstring path);
  void reload_shader();

  void load_selection_shader(czstring path);

  void update_selection();
  void select_face(float x, float y);
  void expand_selection();

  void select_cohomology_group();

 private:
  sf::Vector2i mouse_pos{};
  bool running = false;
  bool view_should_update = false;

  // World Origin
  vec3 origin;
  // Basis Vectors of Right-Handed Coordinate System
  vec3 up{0, 1, 0};
  vec3 right{1, 0, 0};
  vec3 front{0, 0, 1};
  // Spherical/Horizontal Coordinates of Camera
  float radius = 10;
  float altitude = 0;
  float azimuth = 0;

  camera cam{};

  scene surface{};
  future<void> loading_task{};

  vec3 aabb_min{};
  vec3 aabb_max{};
  float bounding_radius;

  opengl::shader_program surface_shader{};
  string surface_shader_path{};

  opengl::element_buffer selection{};
  opengl::shader_program selection_shader{};

  vector<bool> selected_faces{};
  uint32 group = 0;
};

}  // namespace nanoreflex
