#include <nanoreflex/viewer.hpp>
//
#include <nanoreflex/math.hpp>

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
  surface_curve_points.setup();
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
    else if (event.type == sf::Event::MouseButtonPressed) {
      switch (event.mouseButton.button) {
        case sf::Mouse::Middle:
          look_at(event.mouseButton.x, event.mouseButton.y);
          break;
      }
    } else if (event.type == sf::Event::KeyPressed) {
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
        case sf::Keyboard::Num0:
          reset_surface_curve_points();
          break;
        case sf::Keyboard::Space:
          // select_face(mouse_pos.x, mouse_pos.y);
          // add_surface_curve_points(mouse_pos.x, mouse_pos.y);
          break;
        case sf::Keyboard::N:
          expand_selection();
          break;
        case sf::Keyboard::S:
          select_cohomology_group();
          break;
        case sf::Keyboard::X:
          ++group;
          select_cohomology_group();
          break;
        case sf::Keyboard::Y:
          --group;
          select_cohomology_group();
          break;
        case sf::Keyboard::O:
          orientation = !orientation;
          select_oriented_cohomology_group();
          break;
        case sf::Keyboard::F:
          compute_surface_face_curve();
          break;
      }
    }
  }

  if (window.hasFocus()) {
    if (sf::Mouse::isButtonPressed(sf::Mouse::Left))
      turn({-0.01 * mouse_move.x, 0.01 * mouse_move.y});
    if (sf::Mouse::isButtonPressed(sf::Mouse::Right))
      shift({mouse_move.x, mouse_move.y});

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space)) {
      if (mouse_move != sf::Vector2i{})
        add_surface_curve_points(mouse_pos.x, mouse_pos.y);
    }
  }
}

void viewer::update_view() {
  // Computer camera position by using spherical coordinates.
  // This transformation is a variation of the standard
  // called horizontal coordinates often used in astronomy.
  auto p = cos(altitude) * sin(azimuth) * right +  //
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

  surface_curve_point_shader.bind();
  surface_curve_point_shader  //
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
        if (info.oriented() || !surface.edges.contains({e[1], e[0]})) continue;
        cout << "(" << e[0] << ", " << e[1] << ") : " << info.face[0] << ", "
             << info.face[1] << ", " << surface.edges[{e[1], e[0]}].face[0]
             << '\n';
        lines.push_back(e[0]);
        lines.push_back(e[1]);
      }
      edge_selection.allocate_and_initialize(lines);

      loading_task = {};

      cout << "done." << endl
           << "vertices = " << surface.vertices.size() << '\n'
           << "faces = " << surface.faces.size() << '\n'
           << "consistent = " << boolalpha << surface.consistent() << '\n'
           << "oriented = " << boolalpha << surface.oriented() << '\n'
           << "boundary = " << boolalpha << surface.has_boundary() << '\n'
           << "cohomology groups = " << surface.cohomology_group_count << '\n'
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

  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glDepthFunc(GL_ALWAYS);
  selection_shader.bind();
  surface.device_handle.bind();
  selection.bind();
  glDrawElements(GL_TRIANGLES, selection.size(), GL_UNSIGNED_INT, 0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  edge_selection.bind();
  glDrawElements(GL_LINES, edge_selection.size(), GL_UNSIGNED_INT, 0);

  surface_curve_point_shader.bind();
  surface_curve_points.render();
  glDrawArrays(GL_LINE_STRIP, 0, surface_curve_points.vertices.size());
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

void viewer::look_at(float x, float y) {
  const auto r = cam.primary_ray(x, y);
  if (const auto p = intersection(r, surface)) {
    origin = r(p.t);
    radius = p.t;
    view_should_update = true;
  }
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
    surface.generate_face_neighbors();
    surface.generate_cohomology_groups();
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

void viewer::update_selection() {
  decltype(surface.faces) faces{};
  for (size_t i = 0; i < selected_faces.size(); ++i)
    if (selected_faces[i]) faces.push_back(surface.faces[i]);
  selection.allocate_and_initialize(faces);
}

void viewer::select_face(float x, float y) {
  selected_faces.resize(surface.faces.size());
  for (size_t i = 0; i < selected_faces.size(); ++i) selected_faces[i] = false;

  if (const auto p = intersection(cam.primary_ray(x, y), surface)) {
    selected_faces[p.f] = true;
    update_selection();
  }
}

void viewer::expand_selection() {
  auto new_selected_faces = selected_faces;
  for (size_t i = 0; i < selected_faces.size(); ++i) {
    if (!selected_faces[i]) continue;
    for (int j = 0; j < 3; ++j) {
      if (surface.face_neighbors[i][j] == scene::invalid) continue;
      new_selected_faces[surface.face_neighbors[i][j]] = true;
    }
  }
  swap(new_selected_faces, selected_faces);
  update_selection();
}

void viewer::select_cohomology_group() {
  selected_faces.resize(surface.faces.size());
  for (size_t i = 0; i < surface.faces.size(); ++i)
    selected_faces[i] = (surface.cohomology_groups[i] == group);
  update_selection();
}

void viewer::select_oriented_cohomology_group() {
  selected_faces.resize(surface.faces.size());
  for (size_t i = 0; i < surface.faces.size(); ++i)
    selected_faces[i] = (surface.cohomology_groups[i] == group) &&
                        (surface.orientation[i] == orientation);
  update_selection();
}

void viewer::reset_surface_curve_points() {
  surface_curve_points.vertices.clear();
  surface_curve_points.update();
  surface_curve_intersections.clear();
}

void viewer::add_surface_curve_points(float x, float y) {
  const auto r = cam.primary_ray(x, y);
  if (const auto p = intersection(r, surface)) {
    surface_curve_points.vertices.push_back(r(p.t));
    surface_curve_points.update();
    surface_curve_intersections.push_back(p);
  }
}

void viewer::load_surface_curve_point_shader(czstring path) {
  surface_curve_point_shader = opengl::shader_from_file(path);
  view_should_update = true;
}

void viewer::compute_surface_face_curve() {
  surface_face_curve_intersections.clear();
  if (surface_curve_intersections.empty()) return;

  surface_face_curve_intersections.push_back(
      surface_curve_intersections.front());
  size_t count = 1;
  for (size_t i = 1; i < surface_curve_intersections.size(); ++i) {
    const auto& p = surface_curve_intersections[i];
    auto& q = surface_face_curve_intersections.back();

    if (p.f == q.f) {
      q.u += p.u;
      q.v += p.v;
      ++count;
      continue;
    }

    q.u /= count;
    q.v /= count;

    const auto path = surface.shortest_face_path(q.f, p.f);
    for (auto x : path)
      surface_face_curve_intersections.push_back(
          {1.0f / 3.0f, 1.0f / 3.0f, 0.0f, x});
    count = 1;
    if (path.empty()) break;
    surface_face_curve_intersections.back() = p;
  }
  surface_face_curve_intersections.back().u /= count;
  surface_face_curve_intersections.back().v /= count;

  // Provide discrete surface mesh curve.
  curve_start = {surface_face_curve_intersections.front().u,
                 surface_face_curve_intersections.front().v};
  curve_end = {surface_face_curve_intersections.back().u,
               surface_face_curve_intersections.back().v};
  curve_faces.resize(surface_face_curve_intersections.size());
  for (size_t i = 0; i < surface_face_curve_intersections.size(); ++i)
    curve_faces[i] = surface_face_curve_intersections[i].f;
  curve_weights.resize(surface_face_curve_intersections.size() - 1);
  for (size_t i = 1; i < surface_face_curve_intersections.size(); ++i) {
    const auto& p = surface_face_curve_intersections[i - 1];
    const auto& q = surface_face_curve_intersections[i];
    const auto e = surface.common_edge(p.f, q.f);
    curve_weights[i - 1] = edge_weight(
        surface.vertices[e[0]].position, surface.vertices[e[1]].position,
        surface.position(p.f, p.u, p.v), surface.position(q.f, q.u, q.v));
  }

  // Update for Rendering
  surface_curve_points.vertices.clear();
  // for (const auto& p : surface_face_curve_intersections)
  //   surface_curve_points.vertices.push_back(surface.position(p.f, p.u, p.v));
  surface_curve_points.vertices.push_back(
      surface.position(curve_faces.front(), curve_start.x, curve_start.y));
  for (size_t i = 0; i < curve_faces.size() - 1; ++i) {
    const auto e = surface.common_edge(curve_faces[i], curve_faces[i + 1]);
    surface_curve_points.vertices.push_back(  //
        surface.vertices[e[0]].position * (1.0f - curve_weights[i]) +
        surface.vertices[e[1]].position * curve_weights[i]);
  }
  surface_curve_points.vertices.push_back(
      surface.position(curve_faces.back(), curve_end.x, curve_end.y));
  surface_curve_points.update();
}

}  // namespace nanoreflex
