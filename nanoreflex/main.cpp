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
  viewer.load_shader((path / "shader/default").c_str());
  viewer.load_selection_shader((path / "shader/selection").c_str());
  viewer.load_surface_curve_point_shader((path / "shader/points").c_str());
  viewer.run();
}
