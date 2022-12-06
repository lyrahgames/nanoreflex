#include <nanoreflex/viewer.hpp>

namespace nanoreflex {

viewer_context::viewer_context() {
  sf::ContextSettings settings;
  settings.majorVersion = 4;
  settings.minorVersion = 5;
  // These values need to be set when 3D rendering is required.
  settings.depthBits = 24;
  settings.stencilBits = 8;
  settings.antialiasingLevel = 4;

  window.create(sf::VideoMode(800, 450), "Nanoreflex", sf::Style::Default,
                settings);
  window.setVerticalSyncEnabled(true);
  window.setKeyRepeatEnabled(false);
  window.setActive(true);

  glbinding::initialize(sf::Context::getFunction);
}

viewer::viewer() : viewer_context() {
  // To initialize the viewport and matrices,
  // window has to be resized at least once.
  resize();

  // Setup for OpenGL
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_DEPTH_TEST);
  // glEnable(GL_POINT_SMOOTH);
  // glEnable(GL_POINT_SPRITE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(1.0, 1.0, 1.0, 1.0);
  glPointSize(10.0f);
  glLineWidth(4.0f);

  surface.setup();
}

void viewer::resize() {
  const auto s = window.getSize();
  resize(s.x, s.y);
}

void viewer::resize(int width, int height) {
  glViewport(0, 0, width, height);
  cam.set_screen_resolution(width, height);
  view_should_update = true;
}

void viewer::process_events() {
  // Get new mouse position and compute movement in space.
  const auto new_mouse_pos = sf::Mouse::getPosition(window);
  const auto mouse_move = new_mouse_pos - mouse_pos;
  mouse_pos = new_mouse_pos;

  sf::Event event;
  while (window.pollEvent(event)) {
    if (event.type == sf::Event::Closed)
      running = false;
    else if (event.type == sf::Event::Resized)
      resize(event.size.width, event.size.height);
    else if (event.type == sf::Event::MouseWheelScrolled)
      zoom(0.1 * event.mouseWheelScroll.delta);
    else if (event.type == sf::Event::KeyPressed) {
      switch (event.key.code) {
        case sf::Keyboard::Escape:
          running = false;
          break;
        case sf::Keyboard::R:
          reload_shader();
          break;
        case sf::Keyboard::Num1:
          set_y_as_up();
          break;
        case sf::Keyboard::Num2:
          set_z_as_up();
          break;
        case sf::Keyboard::Space:
          select_face(mouse_pos.x, mouse_pos.y);
          break;
      }
    }
  }

  if (window.hasFocus()) {
    if (sf::Mouse::isButtonPressed(sf::Mouse::Left))
      turn({0.01 * mouse_move.x, 0.01 * mouse_move.y});
    if (sf::Mouse::isButtonPressed(sf::Mouse::Right))
      shift({mouse_move.x, mouse_move.y});
  }
}

void viewer::update_view() {
  // Computer camera position by using spherical coordinates.
  // This transformation is a variation of the standard
  // called horizontal coordinates often used in astronomy.
  auto p = cos(altitude) * sin(azimuth) * right -  //
           cos(altitude) * cos(azimuth) * front +  //
           sin(altitude) * up;
  p *= radius;
  p += origin;
  cam.move(p).look_at(origin, up);

  cam.set_near_and_far(std::max(1e-3f * radius, radius - bounding_radius),
                       radius + bounding_radius);

  surface_shader.bind();
  surface_shader  //
      .set("projection", cam.projection_matrix())
      .set("view", cam.view_matrix());

  selection_shader.bind();
  selection_shader  //
      .set("projection", cam.projection_matrix())
      .set("view", cam.view_matrix());
}

void viewer::update() {
  if (loading_task.valid()) {
    if (future_status::ready == loading_task.wait_for(0s)) {
      surface.update();
      fit_view();

      vector<uint32> lines{};
      for (const auto& [e, info] : surface.edges) {
        if (info.oriented()) continue;
        lines.push_back(e[0]);
        lines.push_back(e[1]);
      }
      selection.allocate_and_initialize(lines);

      loading_task = {};

      cout << "done." << endl
           << "vertices = " << surface.vertices.size() << '\n'
           << "faces = " << surface.faces.size() << '\n'
           << "oriented = " << boolalpha << surface.oriented() << '\n'
           << "boundary = " << boolalpha << surface.has_boundary() << '\n'
           << endl;

      cout << "unoriented edges = " << lines.size() << endl;
    } else {
      cout << "." << flush;
    }
  }

  if (view_should_update) {
    update_view();
    view_should_update = false;
  }
}

void viewer::render() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glDepthFunc(GL_LESS);
  surface_shader.bind();
  surface.render();

  glDepthFunc(GL_ALWAYS);
  selection_shader.bind();
  surface.device_handle.bind();
  selection.bind();
  glDrawElements(GL_LINES, selection.size(), GL_UNSIGNED_INT, 0);
}

void viewer::run() {
  running = true;
  while (running) {
    process_events();
    update();
    render();
    window.display();
  }
}

void viewer::turn(const vec2& angle) {
  altitude += angle.y;
  azimuth += angle.x;
  constexpr float bound = pi / 2 - 1e-5f;
  altitude = std::clamp(altitude, -bound, bound);
  view_should_update = true;
}

void viewer::shift(const vec2& pixels) {
  const auto shift = -pixels.x * cam.right() + pixels.y * cam.up();
  const auto scale = cam.pixel_size() * radius;
  origin += scale * shift;
  view_should_update = true;
}

void viewer::zoom(float scale) {
  radius *= exp(-scale);
  view_should_update = true;
}

void viewer::set_z_as_up() {
  right = {1, 0, 0};
  front = {0, -1, 0};
  up = {0, 0, 1};
  view_should_update = true;
}

void viewer::set_y_as_up() {
  right = {1, 0, 0};
  front = {0, 0, 1};
  up = {0, 1, 0};
  view_should_update = true;
}

void viewer::load_scene(czstring file_path) {
  const auto loader = [this](czstring file_path) {
    load_from_file(file_path, surface);
    surface.generate_edges();
    surface.generate_vertex_neighbors();
    // surface.orient();
  };
  loading_task = async(launch::async, loader, file_path);
  cout << "Loading '" << file_path << "'" << flush;
}

void viewer::fit_view() {
  // AABB computation
  aabb_min = {INFINITY, INFINITY, INFINITY};
  aabb_max = {-INFINITY, -INFINITY, -INFINITY};
  for (const auto& vertex : surface.vertices) {
    aabb_min = min(aabb_min, vertex.position);
    aabb_max = max(aabb_max, vertex.position);
  }

  origin = 0.5f * (aabb_max + aabb_min);
  bounding_radius = 0.5f * length(aabb_max - aabb_min);
  radius = 0.5f * length(aabb_max - aabb_min) / tan(0.5f * cam.vfov());
  cam.set_near_and_far(1e-4f * radius, 2 * radius);
  view_should_update = true;
}

void viewer::load_shader(czstring path) {
  surface_shader = opengl::shader_from_file(path);
  surface_shader_path = path;
  view_should_update = true;
}

void viewer::reload_shader() {
  if (surface_shader_path.empty()) return;
  load_shader(surface_shader_path.c_str());
}

void viewer::load_selection_shader(czstring path) {
  selection_shader = opengl::shader_from_file(path);
  view_should_update = true;
}

void viewer::select_face(float x, float y) {
  if (const auto p = intersection(cam.primary_ray(x, y), surface)) {
    const auto& f = surface.faces[p.f];
    array<uint32, 6> lines{f[0], f[1], f[1], f[2], f[2], f[0]};
    selection.allocate_and_initialize(lines);
  }
}

}  // namespace nanoreflex
