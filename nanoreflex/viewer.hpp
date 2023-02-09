#pragma once
#include <nanoreflex/camera.hpp>
#include <nanoreflex/opengl/opengl.hpp>
#include <nanoreflex/points.hpp>
// #include <nanoreflex/polyhedral_surface.hpp>
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

  void load_surface(const filesystem::path& path);
  void handle_surface_load_task();
  void fit_view();

  void print_surface_info();

  void load_shader(czstring path);
  void reload_shader();

  void load_selection_shader(czstring path);

  void update_selection();
  void select_face(float x, float y);
  void expand_selection();

  void select_cohomology_group();
  void select_oriented_cohomology_group();

  void reset_surface_curve_points();
  void add_surface_curve_points(float x, float y);
  void load_surface_curve_point_shader(czstring path);
  void compute_surface_curve_points();

  void compute_surface_face_curve();

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

  // polyhedral_surface surface{};
  scene surface{};

  // The loading of mesh data can take quite a long time
  // and may let the window manager think the program is frozen
  // if the data would be loaded by a blocking call.
  // Here, an asynchronous task is used
  // to get rid of this unresponsiveness.
  future<void> surface_load_task{};
  float32 surface_load_time{};
  float32 surface_process_time{};

  vec3 aabb_min{};
  vec3 aabb_max{};
  float bounding_radius;

  opengl::shader_program surface_shader{};
  string surface_shader_path{};

  opengl::element_buffer selection{};
  opengl::shader_program selection_shader{};

  vector<bool> selected_faces{};
  uint32 group = 0;
  bool orientation = false;

  opengl::element_buffer edge_selection{};

  vector<ray_scene_intersection> surface_curve_intersections{};
  points surface_curve_points{};
  opengl::shader_program surface_curve_point_shader{};

  vector<ray_scene_intersection> surface_face_curve_intersections{};

  vector<uint32> curve_faces{};
  vec2 curve_start, curve_end;
  vector<float> curve_weights{};
};

}  // namespace nanoreflex
