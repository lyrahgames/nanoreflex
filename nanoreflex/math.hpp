#pragma once
#include <nanoreflex/utility.hpp>

namespace nanoreflex {

inline auto edge_weight(vec3 v1, vec3 v2, vec3 p, vec3 q) {
  const auto e = v2 - v1;
  const auto e2 = dot(e, e);

  const auto px = dot(e, p - v1) / e2;
  const auto qx = dot(e, q - v1) / e2;

  const auto pe = px * e + v1;
  const auto qe = qx * e + v1;

  const auto pd = length(p - pe);
  const auto qd = length(q - qe);

  const auto w = 1.0f / (pd + qd);
  const auto w1 = w * qd;
  const auto w2 = w * pd;

  return std::clamp(w1 * px + w2 * qx, 0.0f, 1.0f);
}

}  // namespace nanoreflex
