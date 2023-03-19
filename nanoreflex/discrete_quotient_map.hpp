#pragma once
#include <nanoreflex/utility.hpp>

namespace nanoreflex {

template <unsigned_integral type1, unsigned_integral type2>
struct discrete_quotient_map {
  using domain_type = type1;
  using image_type = type2;

  discrete_quotient_map() = default;
  constexpr discrete_quotient_map(ranges::input_range auto&& input,
                                  image_type count)
      : map(std::forward<decltype(input)>(input)) {
    generate(count);
  }
  constexpr discrete_quotient_map(ranges::input_range auto&& input)
      : discrete_quotient_map(std::forward<decltype(input)>(input),
                              ranges::max(input)) {}

  constexpr auto domain_size() const noexcept { return map.size(); }

  constexpr auto image_size() const noexcept {
    return inverse_offset.size() - 1;
  }

  constexpr void generate(image_type count) {
    assert(count > 0);

    // Get the count of elements per equivalence class.
    inverse_offset.assign(count + 1, 0);
    for (auto y : map) ++inverse_offset[y + 1];

    // Calculate cumulative sum of counts to get offsets.
    for (size_t i = 2; i < inverse_offset.size(); ++i)
      inverse_offset[i] += inverse_offset[i - 1];

    // Generate equivalence classes by using offsets as indices.
    inverse.resize(map.size());
    for (size_t x = 0; x < map.size(); ++x)
      inverse[inverse_offset[map[x]]++] = x;

    //
    for (size_t i = inverse_offset.size() - 1; i > 0; --i)
      inverse_offset[i] = inverse_offset[i - 1];
    inverse_offset[0] = 0;
  }

  constexpr auto operator()(domain_type x) const noexcept -> image_type {
    assert(x < domain_size());
    return map[x];
  }

  constexpr auto operator[](image_type y) const noexcept {
    assert(y < image_size());
    return span(&inverse[inverse_offset[y]], &inverse[inverse_offset[y + 1]]);
  }

  constexpr bool valid() const noexcept {
    for (size_t y = 0; y < inverse_offset.size() - 1; ++y) {
      // for (size_t i = inverse_offset[y]; i < inverse_offset[y + 1]; ++i)
      //   if (y != map[inverse[i]]) return false;
      for (auto x : operator[](y))
        if (y != map[x]) return false;
    }

    auto counts = inverse_offset;
    for (size_t fid = 0; fid < map.size(); ++fid) {
      const auto y = map[fid];
      const auto index = counts[y]++;
      if (fid != inverse[index]) return false;
    }

    return true;
  }

 private:
  vector<domain_type> map{};
  vector<image_type> inverse_offset{};
  vector<image_type> inverse{};
};

}  // namespace nanoreflex
