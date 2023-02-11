#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numbers>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
//
#include <SFML/Graphics.hpp>
//
#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>
//
#include <glm/glm.hpp>
//
#include <glm/ext.hpp>
//
#include <glm/gtx/norm.hpp>

#define NANOREFLEX_ADD_DEFAULT_CONSTRUCTOR_EXTENSION(TYPE)             \
  constexpr auto TYPE##_from(auto&&... args) noexcept(                 \
      noexcept(TYPE(std::forward<decltype(args)>(args)...)))           \
    requires requires { TYPE(std::forward<decltype(args)>(args)...); } \
  {                                                                    \
    return TYPE(std::forward<decltype(args)>(args)...);                \
  }

namespace nanoreflex {

using namespace std;
using namespace gl;
using namespace glm;

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using float32 = float;
using float64 = double;
using real = float32;
using czstring = const char*;

using clock = chrono::high_resolution_clock;
using chrono::duration;

constexpr auto pi = std::numbers::pi_v<real>;
constexpr auto infinity = std::numeric_limits<real>::infinity();

inline auto last_changed(const filesystem::path& path) {
  auto time = last_write_time(path);
  for (const auto& entry : filesystem::recursive_directory_iterator(path))
    time = std::max(time, entry.last_write_time());
  return time;
}

}  // namespace nanoreflex
