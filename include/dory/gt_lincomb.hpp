#pragma once

#include "dory/backend.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dory {

// PLAN §14.1: one symbolic target-group exponentiation term, base^scalar.
struct GtTerm {
    GT base;
    Fr scalar;
};

// PLAN §14.1: symbolic GT multi-exponentiation.
// Represents product_i base_i^scalar_i without evaluating until the final check.
using GtLinComb = std::vector<GtTerm>;

enum class GtMultiexpMethod {
    Simple,
    WindowedBucket
};

void append_term(GtLinComb& out, const GT& base, const Fr& scalar);

void append_scaled_terms(
    GtLinComb& out,
    const GtLinComb& in,
    const Fr& scale);

// Milestone 6b: remove zero terms and identity bases without unsafe GT hashing.
GtLinComb normalize_gt_lincomb(const GtLinComb& terms);

// Milestone 6b: canonical little-endian scalar words via mcl::Fr::getLittleEndian.
std::vector<std::uint64_t> fr_to_words_le(const Fr& scalar);

std::uint64_t get_window(
    const Fr& scalar,
    std::size_t window_index,
    std::size_t window_bits);

std::size_t gt_multiexp_window_bits(std::size_t term_count);
GtMultiexpMethod choose_gt_multiexp_method(std::size_t term_count);

// Milestone 6b: retained correctness oracle using independent exponentiations.
GT eval_gt_multiexp_simple(const GtLinComb& terms);

// Milestone 6b: multiplicative-GT windowed bucket simultaneous exponentiation.
GT eval_gt_multiexp_windowed_bucket(const GtLinComb& terms);

GT eval_gt_multiexp(const GtLinComb& terms, GtMultiexpMethod method);

// PLAN §14.1: policy-dispatched evaluator used by the deferred verifier.
GT eval_gt_multiexp(const GtLinComb& terms);

} // namespace dory
