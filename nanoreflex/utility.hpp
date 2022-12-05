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
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
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

namespace nanoreflex {

using namespace std;
using namespace gl;
using namespace glm;

using uint32 = uint32_t;
using real = float;
using czstring = const char*;

constexpr auto pi = std::numbers::pi_v<real>;

}  // namespace nanoreflex
