#pragma once

#include <mcl/bn.hpp>

#include <string_view>
#include <vector>

namespace dory {

// PLAN §1 and §4: these aliases are the complete curve-facing vocabulary used
// above the backend boundary, which keeps a later curve/backend port localized.
using Fr = mcl::bn::Fr;
using G1 = mcl::bn::G1;
using G2 = mcl::bn::G2;
using GT = mcl::bn::Fp12;

struct Backend {
    static void init_bn254();
};

Fr random_fr();
Fr random_nonzero_fr();

bool is_zero(const Fr& x);
Fr zero_fr();
Fr one_fr();
Fr neg(const Fr& x);
Fr inv(const Fr& x);

G1 g1_zero();
G2 g2_zero();

G1 g1_generator();
G2 g2_generator();

// PLAN §7 test setup: derive independent public points from domain-separated
// labels while keeping mcl-specific hash-to-curve calls behind this wrapper.
G1 hash_to_g1_for_tests(std::string_view label);
G2 hash_to_g2_for_tests(std::string_view label);

// SECURITY NOTE:
// random_g1_for_tests() and random_g2_for_tests() may sample a scalar and multiply
// a fixed generator. This is acceptable for tests only. For transparent
// production setup, public generators should be hash-to-curve/domain-separated
// so no setup party knows their discrete logarithms.
G1 random_g1_for_tests();
G2 random_g2_for_tests();

G1 g1_add(const G1& a, const G1& b);
G2 g2_add(const G2& a, const G2& b);

G1 g1_mul(const G1& a, const Fr& s);
G2 g2_mul(const G2& a, const Fr& s);

bool g1_equal(const G1& a, const G1& b);
bool g2_equal(const G2& a, const G2& b);

GT gt_one();
GT gt_mul(const GT& a, const GT& b);
GT gt_pow(const GT& a, const Fr& s);
GT gt_inv(const GT& a);
bool gt_equal(const GT& a, const GT& b);

GT pair(const G1& a, const G2& b);

enum class PairProductMethod {
    Auto,
    Simple,
    MultiPairing
};

GT pair_product_simple(const std::vector<G1>& A, const std::vector<G2>& B);
GT pair_product_multipairing(const std::vector<G1>& A, const std::vector<G2>& B);
GT pair_product(
    const std::vector<G1>& A,
    const std::vector<G2>& B,
    PairProductMethod method);

PairProductMethod choose_pair_product_method(std::size_t term_count);
void set_pair_product_method(PairProductMethod method);
PairProductMethod get_pair_product_method();

GT pair_product(const std::vector<G1>& A, const std::vector<G2>& B);

} // namespace dory
