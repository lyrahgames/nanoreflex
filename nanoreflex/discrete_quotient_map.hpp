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
      : labels(std::forward<decltype(input)>(input)) {
    generate(count);
  }
  constexpr discrete_quotient_map(ranges::input_range auto&& input)
      : discrete_quotient_map(std::forward<decltype(input)>(input),
                              ranges::max(input)) {}

  discrete_quotient_map(ranges::input_range auto&& input,
                        auto&& equal,
                        auto&& hash) {
    using hasher = decay_t<decltype(hash)>;
    using equaler = decay_t<decltype(equal)>;
    unordered_map<domain_type, image_type, hasher, equaler> indices(
        {}, forward<decltype(hash)>(hash), forward<decltype(equal)>(equal));
    indices.reserve(ranges::size(input));

    labels.resize(ranges::size(input));
    image_type label_count = 0;

    for (size_t i = 0; const auto& x : input) {
      const auto it = indices.find(x);
      if (it != end(indices))
        labels[i] = it->second;
      else {
        labels[i] = label_count;
        indices.emplace(x, label_count++);
      }
      ++i;
    }

    generate(label_count);
  }

  constexpr auto domain_size() const noexcept { return labels.size(); }

  constexpr auto image_size() const noexcept {
    return inverse_offset.size() - 1;
  }

  constexpr void generate(image_type count) {
    assert(count > 0);

    // Get the count of elements per equivalence class.
    inverse_offset.assign(count + 1, 0);
    for (auto y : labels) ++inverse_offset[y + 1];

    // Calculate cumulative sum of counts to get offsets.
    for (size_t i = 2; i < inverse_offset.size(); ++i)
      inverse_offset[i] += inverse_offset[i - 1];

    // Generate equivalence classes by using offsets as indices.
    inverse.resize(labels.size());
    for (size_t x = 0; x < labels.size(); ++x)
      inverse[inverse_offset[labels[x]]++] = x;

    //
    for (size_t i = inverse_offset.size() - 1; i > 0; --i)
      inverse_offset[i] = inverse_offset[i - 1];
    inverse_offset[0] = 0;
  }

  constexpr auto operator()(domain_type x) const noexcept -> image_type {
    assert(x < domain_size());
    return labels[x];
  }

  constexpr auto operator[](image_type y) const noexcept {
    assert(y < image_size());
    return span(&inverse[inverse_offset[y]], &inverse[inverse_offset[y + 1]]);
  }

  constexpr bool valid() const noexcept {
    for (size_t y = 0; y < inverse_offset.size() - 1; ++y) {
      // for (size_t i = inverse_offset[y]; i < inverse_offset[y + 1]; ++i)
      //   if (y != labels[inverse[i]]) return false;
      for (auto x : operator[](y))
        if (y != labels[x]) return false;
    }

    auto counts = inverse_offset;
    for (size_t fid = 0; fid < labels.size(); ++fid) {
      const auto y = labels[fid];
      const auto index = counts[y]++;
      if (fid != inverse[index]) return false;
    }

    return true;
  }

 private:
  vector<domain_type> labels{};
  vector<image_type> inverse_offset{};
  vector<image_type> inverse{};
};

}  // namespace nanoreflex
