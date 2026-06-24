#include "dory/vector_ops.hpp"

#include <stdexcept>

namespace dory {

bool is_power_of_two(std::size_t n) {
    // PLAN §3 and §6: Dory vector lengths must be nonzero powers of two.
    return n != 0U && (n & (n - 1U)) == 0U;
}

std::size_t log2_exact(std::size_t n) {
    if (!is_power_of_two(n)) {
        throw std::invalid_argument("log2_exact requires a nonzero power of two");
    }

    std::size_t result = 0;
    while (n > 1U) {
        n >>= 1U;
        ++result;
    }
    return result;
}

GT commit_pairing(
    const std::vector<G1>& A,
    const std::vector<G2>& B,
    const Fr& r,
    const GT& Q) {
    // PLAN §6 equation: commit_pairing(A,B,r,Q) = e(A,B) * Q^r.
    // GT is multiplicative, so both the product and exponent remain literal.
    return gt_mul(pair_product(A, B), gt_pow(Q, r));
}

GT commit_pairing_scalar(const G1& A, const G2& B, const Fr& r, const GT& Q) {
    // PLAN §6 equation: commit_pairing_scalar(A,B,r,Q) = e(A,B) * Q^r.
    // GT is multiplicative, so both the product and exponent remain literal.
    return gt_mul(pair(A, B), gt_pow(Q, r));
}

} // namespace dory

