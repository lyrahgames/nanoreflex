#pragma once
#include <nanoreflex/camera.hpp>
#include <nanoreflex/opengl/opengl.hpp>
#include <nanoreflex/points.hpp>
#include <nanoreflex/polyhedral_surface.hpp>
#include <nanoreflex/shader_manager.hpp>
#include <nanoreflex/surface_mesh_curve.hpp>
#include <nanoreflex/utility.hpp>

namespace nanoreflex {

// To be able to use OpenGL utilities
// in the state of the Viewer,
// an OpenGL context needs to be created first.
// We enforce this by inheriting from the context structure.
//
class viewer_context {
 public:
  viewer_context();

  void info(const auto& data) { cout << "INFO:\n" << data << endl; }
  void error(const auto& data) { cout << "ERROR:\n" << data << endl; }

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

  void load_shader(const filesystem::path& path, const string& name);

  // void load_surface_shader(const filesystem::path& path);
  // void reload_surface_shader();

  // void load_selection_shader(const filesystem::path& path);

  void update_selection();
  void select_face(float x, float y);
  void expand_selection();

  // void select_cohomology_group();
  void select_component();
  // void select_oriented_cohomology_group();

  void reset_surface_curve_points();
  void add_surface_curve_points(float x, float y);
  // void load_surface_curve_point_shader(czstring path);
  void compute_surface_curve_points();

  void close_surface_curve();

  void sort_surface_faces_by_depth();

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
  // v2::scene surface2{};

  // The loading of mesh data can take quite a long time
  // and may let the window manager think the program is frozen
  // if the data would be loaded by a blocking call.
  // Here, an asynchronous task is used
  // to get rid of this unresponsiveness.
  future<void> surface_load_task{};
  float32 surface_load_time{};
  float32 surface_process_time{};
  //
  float bounding_radius;
  //
  // opengl::shader_program surface_shader{};
  // filesystem::path surface_shader_path{};
  // filesystem::file_time_type surface_shader_time{};

  opengl::element_buffer surface_boundary{};
  opengl::element_buffer surface_unoriented_edges{};
  opengl::element_buffer surface_inconsistent_edges{};

  shader_manager shaders{};

  opengl::element_buffer selection{};
  // opengl::shader_program selection_shader{};

  vector<bool> selected_faces{};
  uint32 group = 0;
  bool orientation = false;

  opengl::element_buffer edge_selection{};

  surface_mesh_curve curve{};
  points surface_curve_points{};
  surface_mesh_curve smooth_curve{};
  points smooth_curve_points{};
};

}  // namespace nanoreflex
