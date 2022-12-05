#include <nanoreflex/viewer.hpp>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "Usage:\n" << argv[0] << " <STL object file path>\n";
    return 0;
  }

  nanoreflex::viewer viewer{};
  viewer.load_scene(argv[1]);
  viewer.load_shader((std::filesystem::path(argv[0]).parent_path() /
                      std::filesystem::path("shader/default"))
                         .c_str());
  viewer.run();
}
