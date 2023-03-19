#include <nanoreflex/viewer.hpp>

using namespace std;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "Usage:\n" << argv[0] << " <STL object file path>\n";
    return 0;
  }

  const auto path = filesystem::path(argv[0]).parent_path();

  nanoreflex::viewer viewer{};
  viewer.load_surface(argv[1]);

  viewer.load_shader(path / "shader/default", "default");
  viewer.load_shader(path / "shader/wireframe", "flat");
  viewer.load_shader(path / "shader/points", "points");
  viewer.load_shader(path / "shader/initial", "initial");
  viewer.load_shader(path / "shader/critical", "critical");
  viewer.load_shader(path / "shader/contours", "contours");
  viewer.load_shader(path / "shader/selection", "selection");
  viewer.load_shader(path / "shader/boundary", "boundary");
  viewer.load_shader(path / "shader/unoriented", "unoriented");
  viewer.load_shader(path / "shader/inconsistent", "inconsistent");

  // viewer.load_surface_shader(path / "shader/default");
  // viewer.load_selection_shader(path / "shader/selection");
  // viewer.load_surface_curve_point_shader((path / "shader/points").c_str());
  viewer.run();
}
