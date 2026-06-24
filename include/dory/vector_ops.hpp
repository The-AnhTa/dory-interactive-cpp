#pragma once

#include "dory/backend.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dory {

bool is_power_of_two(std::size_t n);
std::size_t log2_exact(std::size_t n);

// PLAN §6: protocol vectors are divided into equally sized left/right halves.
// Reject odd lengths here rather than silently dropping an element.
template <typename T>
std::pair<std::vector<T>, std::vector<T>> split_half(const std::vector<T>& v) {
    if ((v.size() & 1U) != 0U) {
        throw std::invalid_argument("split_half requires an even-length vector");
    }
    const auto middle = v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2U);
    return {{v.begin(), middle}, {middle, v.end()}};
}

GT commit_pairing(
    const std::vector<G1>& A,
    const std::vector<G2>& B,
    const Fr& r,
    const GT& Q);

GT commit_pairing_scalar(const G1& A, const G2& B, const Fr& r, const GT& Q);

} // namespace dory

