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

  window.create(sf::VideoMode(600, 600), "Nanoreflex", sf::Style::Default,
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
          reload_surface_shader();
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
          // select_cohomology_group();
          select_connection_group();
          break;
        case sf::Keyboard::X:
          ++group;
          // select_cohomology_group();
          select_connection_group();
          break;
        case sf::Keyboard::Y:
          --group;
          // select_cohomology_group();
          select_connection_group();
          break;
        // case sf::Keyboard::O:
        //   orientation = !orientation;
        //   select_oriented_cohomology_group();
        //   break;
        case sf::Keyboard::Z:
          sort_surface_faces_by_depth();
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
      if (mouse_move != sf::Vector2i{}) {
        add_surface_curve_points(mouse_pos.x, mouse_pos.y);
        compute_surface_curve_points();
      }
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

  shaders.apply([this](opengl::shader_program_handle shader) {
    shader.bind()
        .set("projection", cam.projection_matrix())
        .set("view", cam.view_matrix())
        .try_set("viewport", cam.viewport_matrix());
  });
}

void viewer::update() {
  handle_surface_load_task();
  if (view_should_update) {
    update_view();
    view_should_update = false;
  }

  if (surface_shader_time != last_changed(surface_shader_path)) {
    info("Surface shader has changed on disk. Reloading triggered.");
    reload_surface_shader();
  }

  shaders.reload([this](opengl::shader_program_handle shader) {
    shader.bind()
        .set("projection", cam.projection_matrix())
        .set("view", cam.view_matrix())
        .try_set("viewport", cam.viewport_matrix());
  });
}

void viewer::render() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glDepthFunc(GL_LESS);
  // surface_shader.bind();
  shaders.names["flat"]->second.shader.bind();
  // surface.render();
  surface.render();

  // shaders.names["contours"]->second.shader.bind();
  // surface.render();

  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glDepthFunc(GL_ALWAYS);
  // selection_shader.bind();
  shaders.names["selection"]->second.shader.bind();
  // surface.device_handle.bind();
  surface.device_handle.bind();
  selection.bind();
  glDrawElements(GL_TRIANGLES, selection.size(), GL_UNSIGNED_INT, 0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  edge_selection.bind();
  glDrawElements(GL_LINES, edge_selection.size(), GL_UNSIGNED_INT, 0);

  // surface_curve_point_shader.bind();
  shaders.names["points"]->second.shader.bind();
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

void viewer::load_surface(const filesystem::path& path) {
  const auto loader = [this](const filesystem::path& path) {
    try {
      const auto start = clock::now();
      surface.host() = v2::polyhedral_surface_from(path);
      const auto mid = clock::now();
      // surface.generate_edges();
      // surface.generate_vertex_neighbors();
      // surface.generate_face_neighbors();
      // surface.generate_cohomology_groups();

      surface.generate_topological_vertices();
      surface.generate_edges();
      surface.generate_face_neighbors();
      surface.generate_connection_groups();
      const auto end = clock::now();

      // Evaluate loading and processing time.
      surface_load_time = duration<float32>(mid - start).count();
      surface_process_time = duration<float32>(end - mid).count();
    } catch (exception& e) {
      cout << "failed.\n" << e.what() << endl;
      return;
    }
  };
  surface_load_task = async(launch::async, loader, path);
  cout << "Loading " << path << flush;
}

void viewer::handle_surface_load_task() {
  if (!surface_load_task.valid()) return;
  if (future_status::ready != surface_load_task.wait_for(0s)) {
    cout << "." << flush;
    return;
  }
  cout << "done." << endl << '\n';
  surface_load_task = {};

  // surface.update();
  surface.update();
  fit_view();
  print_surface_info();

  // vector<uint32> lines{};
  // for (const auto& [e, info] : surface.edges) {
  //   if (!surface.oriented() || surface.edges.contains({e[1], e[0]})) continue;
  //   // cout << "(" << e[0] << ", " << e[1] << ") : " << info.face[0] << ", "
  //   //      << info.face[1] << ", " << surface.edges[{e[1], e[0]}].face[0] << '\n';
  //   lines.push_back(e[0]);
  //   lines.push_back(e[1]);
  // }
  // edge_selection.allocate_and_initialize(lines);
  // cout << "unoriented edges = " << lines.size() << endl;
}

void viewer::fit_view() {
  const auto box = aabb_from(surface);
  origin = box.origin();
  bounding_radius = box.radius();
  radius = bounding_radius / tan(0.5f * cam.vfov());
  cam.set_near_and_far(1e-4f * radius, 2 * radius);
  view_should_update = true;
}

void viewer::print_surface_info() {
  constexpr auto left_width = 20;
  constexpr auto right_width = 10;
  cout << setprecision(3) << fixed << boolalpha;
  cout << setw(left_width) << "load time"
       << " = " << setw(right_width) << surface_load_time << " s\n"
       << setw(left_width) << "process time"
       << " = " << setw(right_width) << surface_process_time << " s\n"
       << '\n';
  // << setw(left_width) << "vertices"
  // << " = " << setw(right_width) << surface.vertices.size() << '\n'
  // << setw(left_width) << "faces"
  // << " = " << setw(right_width) << surface.faces.size() << '\n'
  // << setw(left_width) << "consistent"
  // << " = " << setw(right_width) << surface.consistent() << '\n'
  // << setw(left_width) << "oriented"
  // << " = " << setw(right_width) << surface.oriented() << '\n'
  // << setw(left_width) << "boundary"
  // << " = " << setw(right_width) << surface.has_boundary() << '\n'
  // << setw(left_width) << "cohomology groups"
  // << " = " << setw(right_width) << surface.cohomology_group_count << '\n'
  // << endl;

  cout << setw(left_width) << "vertices"
       << " = " << setw(right_width) << surface.vertices.size() << '\n'
       << setw(left_width) << "faces"
       << " = " << setw(right_width) << surface.faces.size() << '\n'
       << setw(left_width) << "consistent"
       << " = " << setw(right_width) << surface.consistent() << '\n'
       << setw(left_width) << "oriented"
       << " = " << setw(right_width) << surface.oriented() << '\n'
       << setw(left_width) << "boundary"
       << " = " << setw(right_width) << surface.has_boundary() << '\n'
       << setw(left_width) << "connection groups"
       << " = " << setw(right_width) << surface.connection_group_count << '\n'
       << endl;
}

void viewer::load_shader(const filesystem::path& path, const string& name) {
  shaders.load_shader(path);
  shaders.add_name(path, name);
}

void viewer::load_surface_shader(const filesystem::path& path) {
  try {
    surface_shader_time = last_changed(path);
    surface_shader = opengl::shader_from_file(path);
    surface_shader_path = path;
    view_should_update = true;
  } catch (runtime_error& e) {
    error("Failed to load surface shader from path '"s + path.string() +
          "'.\n" + e.what());
  }
}

void viewer::reload_surface_shader() {
  if (surface_shader_path.empty()) return;
  load_surface_shader(surface_shader_path);
}

void viewer::load_selection_shader(const filesystem::path& path) {
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

// void viewer::select_cohomology_group() {
//   selected_faces.resize(surface.faces.size());
//   for (size_t i = 0; i < surface.faces.size(); ++i)
//     selected_faces[i] = (surface.cohomology_groups[i] == group);
//   update_selection();
// }

void viewer::select_connection_group() {
  selected_faces.resize(surface.faces.size());
  for (size_t i = 0; i < surface.faces.size(); ++i)
    selected_faces[i] = (surface.connection_groups[i] == group);
  update_selection();
}

// void viewer::select_oriented_cohomology_group() {
//   selected_faces.resize(surface.faces.size());
//   for (size_t i = 0; i < surface.faces.size(); ++i)
//     selected_faces[i] = (surface.cohomology_groups[i] == group) &&
//                         (surface.orientation[i] == orientation);
//   update_selection();
// }

void viewer::reset_surface_curve_points() {
  surface_curve_points.vertices.clear();
  surface_curve_points.update();
  // surface_curve_intersections.clear();
  curve_faces.clear();
  curve_weights.clear();
}

void viewer::add_surface_curve_points(float x, float y) {
  const auto r = cam.primary_ray(x, y);
  const auto p = intersection(r, surface);
  if (!p) return;

  if (curve_faces.empty()) {
    curve_faces.push_back(p.f);
    curve_start = {p.u, p.v};
    curve_end = {p.u, p.v};
    return;
  }

  const auto remove_artifacts = [&](uint32 f) {
    const auto fid = curve_faces.back();
    if (f == fid) {
      curve_end = {p.u, p.v};
      return true;
    }
    if (curve_faces.size() >= 2) {
      const auto fid2 = curve_faces[curve_faces.size() - 2];
      if (f == fid2) {
        curve_faces.pop_back();
        curve_weights.pop_back();
        return true;
      }
    }
    return false;
  };

  if (remove_artifacts(p.f)) return;
  const auto fid = curve_faces.back();
  const auto path = surface.shortest_face_path(fid, p.f);

  // Compute edge weights
  // if (path.size() == 1) {
  //   const auto fid1 = curve_faces.back();
  //   const auto fid2 = path[0];
  //   const auto e = surface.common_edge(fid1, fid2);
  //   const auto v1 = surface.vertices[e[0]].position;
  //   const auto v2 = surface.vertices[e[1]].position;
  //   const auto x = surface.position(fid1, 1.0f / 3, 1.0f / 3);
  //   const auto y = surface.position(fid2, p.u, p.v);
  //   const auto w = edge_weight(v1, v2, x, y);
  //   if (!remove_artifacts(fid2)) {
  //     curve_faces.push_back(fid2);
  //     curve_weights.push_back(w);
  //   }
  // } else if (path.size() == 2) {
  //   const auto fid1 = curve_faces.back();
  //   const auto fid2 = path[0];
  //   const auto fid3 = path[1];
  //   const auto edge1 = surface.common_edge(fid1, fid2);
  //   const auto edge2 = surface.common_edge(fid2, fid3);
  //   const auto o1 = surface.vertices[edge1[0]].position;
  //   const auto o2 = surface.vertices[edge2[0]].position;
  //   const auto e1 = normalize(surface.vertices[edge1[1]].position - o1);
  //   const auto e2 = normalize(surface.vertices[edge2[1]].position - o2);
  //   const auto e1l = length(surface.vertices[edge1[1]].position - o1);
  //   const auto e2l = length(surface.vertices[edge2[1]].position - o2);

  //   if (edge1[0] == edge2[0])
  //     cout << "left rotation" << endl;
  //   else
  //     cout << "right rotation" << endl;

  //   const auto sign = (edge1[0] == edge2[0]) ? 1.0f : -1.0f;

  //   const auto e1x = dot(e1, e2);
  //   const auto e1y = sign * length(e1 - e1x * e2);
  //   const auto e2x = e1x;
  //   const auto e2y = sign * length(e2 - e2x * e1);

  //   const auto x = surface.position(fid1, curve_end.x, curve_end.y);
  //   // const auto x = surface.position(fid1, 1.0f / 3, 1.0f / 3);
  //   const auto x1 = x - o1;
  //   const auto x1x = dot(x1, e1);
  //   const auto x1y = length(x1 - x1x * e1);

  //   const auto y = surface.position(fid3, p.u, p.v);
  //   // const auto y = surface.position(fid3, 1.0f / 3, 1.0f / 3);
  //   const auto y2 = y - o2;
  //   const auto y2x = dot(y2, e2);
  //   const auto y2y = length(y2 - y2x * e2);

  //   const auto s = o2 - o1;
  //   const auto s1x = dot(s, e1);
  //   const auto s1y = length(s - s1x * e1);
  //   const auto s2x = dot(s, e2);
  //   const auto s2y = -length(s - s2x * e2);

  //   const auto x2x = e1x * x1x - e1y * x1y - s2x;
  //   const auto x2y = e1y * x1x + e1x * x1y - s2y;
  //   const auto y1x = e2x * y2x - e2y * y2y + s1x;
  //   const auto y1y = e2y * y2x + e2x * y2y + s1y;

  //   const auto w1 = 1.0f / ((x1y) + (y1y));
  //   const auto w1x = w1 * (y1y);
  //   const auto w1y = w1 * (x1y);
  //   const auto t1 = (w1x * x1x + w1y * y1x) / e1l;
  //   const auto r1 = std::clamp(t1, 0.0f, 1.0f);

  //   const auto w2 = 1.0f / ((x2y) + (y2y));
  //   const auto w2x = w2 * (y2y);
  //   const auto w2y = w2 * (x2y);
  //   const auto t2 = (w2x * x2x + w2y * y2x) / e2l;
  //   const auto r2 = std::clamp(t2, 0.0f, 1.0f);

  //   if (!remove_artifacts(fid2)) {
  //     curve_faces.push_back(fid2);
  //     curve_weights.push_back(r1);
  //   }
  //   if (!remove_artifacts(fid3)) {
  //     curve_faces.push_back(fid3);
  //     curve_weights.push_back(r2);
  //   }
  // } else {
  for (auto x : path) {
    if (remove_artifacts(x)) continue;
    curve_faces.push_back(x);
    curve_weights.push_back(0.5f);
  }
  // }
  curve_end = {p.u, p.v};
}

void viewer::compute_surface_curve_points() {
  // Update for Rendering
  surface_curve_points.vertices.clear();
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

void viewer::load_surface_curve_point_shader(czstring path) {
  surface_curve_point_shader = opengl::shader_from_file(path);
  view_should_update = true;
}

void viewer::sort_surface_faces_by_depth() {
  auto faces = surface.faces;
  sort(begin(faces), end(faces), [&](const auto& f1, const auto& f2) {
    const auto& v = surface.vertices;
    const auto p1 =
        (v[f1[0]].position + v[f1[1]].position + v[f1[2]].position) / 3.0f;
    const auto d1 = length(cam.position() - p1);
    const auto p2 =
        (v[f2[0]].position + v[f2[1]].position + v[f2[2]].position) / 3.0f;
    const auto d2 = length(cam.position() - p2);
    return d1 > d2;
  });
  // surface.device_faces.allocate_and_initialize(faces);
  surface.device_faces.allocate_and_initialize(faces);
}

}  // namespace nanoreflex
