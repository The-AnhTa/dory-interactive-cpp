#include "dory/gt_lincomb.hpp"

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dory {
namespace {

std::uint64_t get_window_from_words(
    const std::vector<std::uint64_t>& words,
    std::size_t window_index,
    std::size_t window_bits) {
    if (window_bits == 0U || window_bits > 63U) {
        throw std::invalid_argument("window_bits must be in [1,63]");
    }
    const std::size_t bit_offset = window_index * window_bits;
    const std::size_t word_index = bit_offset / 64U;
    const std::size_t shift = bit_offset % 64U;
    if (word_index >= words.size()) {
        return 0U;
    }

    std::uint64_t value = words[word_index] >> shift;
    if (shift != 0U && word_index + 1U < words.size()) {
        value |= words[word_index + 1U] << (64U - shift);
    }
    const std::uint64_t mask = (std::uint64_t{1} << window_bits) - 1U;
    return value & mask;
}

} // namespace

void append_term(GtLinComb& out, const GT& base, const Fr& scalar) {
    // PLAN §14.1: append one unevaluated factor base^scalar.
    out.push_back(GtTerm{base, scalar});
}

void append_scaled_terms(
    GtLinComb& out,
    const GtLinComb& in,
    const Fr& scale) {
    // PLAN §14.1: scale product_i X_i^a_i by s symbolically, producing
    // product_i X_i^(s*a_i), without any target-group exponentiation here.
    for (const GtTerm& term : in) {
        append_term(out, term.base, term.scalar * scale);
    }
}

GtLinComb normalize_gt_lincomb(const GtLinComb& terms) {
    GtLinComb normalized;
    normalized.reserve(terms.size());
    const GT identity = gt_one();
    for (const GtTerm& term : terms) {
        // Milestone 6b: X^0 and 1_GT^a are identity factors and can be removed.
        if (!is_zero(term.scalar) && !gt_equal(term.base, identity)) {
            normalized.push_back(term);
        }
    }
    return normalized;
}

std::vector<std::uint64_t> fr_to_words_le(const Fr& scalar) {
    // Milestone 6b: mcl explicitly exports the canonical, non-Montgomery field
    // representative as little-endian bytes. This correctly maps Fr(-r) to q-r.
    const std::size_t byte_capacity = (Fr::getModBitLen() + 7U) / 8U;
    std::vector<std::uint8_t> bytes(byte_capacity, 0U);
    const std::size_t written = scalar.getLittleEndian(bytes.data(), bytes.size());
    if (written == 0U) {
        throw std::runtime_error("mcl failed to export canonical Fr bytes");
    }

    std::vector<std::uint64_t> words((byte_capacity + 7U) / 8U, 0U);
    for (std::size_t i = 0; i < written; ++i) {
        words[i / 8U] |= static_cast<std::uint64_t>(bytes[i]) << (8U * (i % 8U));
    }
    return words;
}

std::uint64_t get_window(
    const Fr& scalar,
    std::size_t window_index,
    std::size_t window_bits) {
    return get_window_from_words(fr_to_words_le(scalar), window_index, window_bits);
}

std::size_t gt_multiexp_window_bits(std::size_t term_count) {
    if (term_count < 32U) {
        return 3U;
    }
    if (term_count < 128U) {
        return 4U;
    }
    return 5U;
}

GtMultiexpMethod choose_gt_multiexp_method(std::size_t term_count) {
    // Milestone 6b measured policy: simple wins below the 43-term n=8 case,
    // while windowed is consistently faster by the 54-term n=16 case.
    return term_count < 48U
        ? GtMultiexpMethod::Simple
        : GtMultiexpMethod::WindowedBucket;
}

GT eval_gt_multiexp_simple(const GtLinComb& terms) {
    // PLAN §14.1 reference: eval(A) = product_{(X,a) in A} X^a.
    GT acc = gt_one();
    for (const GtTerm& term : terms) {
        // Milestone 6b oracle: retain one independent full exponentiation per term.
        acc = gt_mul(acc, gt_pow(term.base, term.scalar));
    }
    return acc;
}

GT eval_gt_multiexp_windowed_bucket(const GtLinComb& terms) {
    const GtLinComb normalized = normalize_gt_lincomb(terms);
    if (normalized.empty()) {
        return gt_one();
    }

    const std::size_t window_bits = gt_multiexp_window_bits(normalized.size());
    const std::size_t bucket_count = (std::size_t{1} << window_bits) - 1U;
    const std::size_t window_count =
        (Fr::getModBitLen() + window_bits - 1U) / window_bits;

    std::vector<std::vector<std::uint64_t>> scalars;
    scalars.reserve(normalized.size());
    for (const GtTerm& term : normalized) {
        scalars.push_back(fr_to_words_le(term.scalar));
    }

    GT result = gt_one();
    for (std::size_t window = window_count; window-- > 0U;) {
        // Milestone 6b: shift the accumulated exponent by 2^window_bits.
        for (std::size_t bit = 0; bit < window_bits; ++bit) {
            result = gt_mul(result, result);
        }

        std::vector<GT> buckets(bucket_count, gt_one());
        for (std::size_t i = 0; i < normalized.size(); ++i) {
            const std::uint64_t chunk = get_window_from_words(
                scalars[i], window, window_bits);
            if (chunk != 0U) {
                buckets[static_cast<std::size_t>(chunk - 1U)] = gt_mul(
                    buckets[static_cast<std::size_t>(chunk - 1U)],
                    normalized[i].base);
            }
        }

        GT running = gt_one();
        GT window_acc = gt_one();
        for (std::size_t bucket = bucket_count; bucket-- > 0U;) {
            running = gt_mul(running, buckets[bucket]);
            window_acc = gt_mul(window_acc, running);
        }
        result = gt_mul(result, window_acc);
    }
    return result;
}

GT eval_gt_multiexp(const GtLinComb& terms, GtMultiexpMethod method) {
    switch (method) {
    case GtMultiexpMethod::Simple:
        return eval_gt_multiexp_simple(terms);
    case GtMultiexpMethod::WindowedBucket:
        return eval_gt_multiexp_windowed_bucket(terms);
    }
    throw std::invalid_argument("unknown GT multi-exponentiation method");
}

GT eval_gt_multiexp(const GtLinComb& terms) {
    return eval_gt_multiexp(terms, choose_gt_multiexp_method(terms.size()));
}

} // namespace dory
