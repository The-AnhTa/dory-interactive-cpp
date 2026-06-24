#include "dory/backend.hpp"
#include "dory/vector_ops.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_bilinearity() {
    const dory::G1 P = dory::g1_generator();
    const dory::G2 Q = dory::g2_generator();
    const dory::Fr a = dory::random_nonzero_fr();
    const dory::Fr b = dory::random_nonzero_fr();

    // PLAN §17.1 equation: e(aP,bQ) = e(P,Q)^(a*b).
    // G1/G2 scalar multiplication is additive notation; GT exponentiation is
    // multiplicative notation.
    const dory::GT lhs = dory::pair(dory::g1_mul(P, a), dory::g2_mul(Q, b));
    const dory::GT rhs = dory::gt_pow(dory::pair(P, Q), a * b);
    require(dory::gt_equal(lhs, rhs), "pairing bilinearity failed");
}

void test_pair_product() {
    std::vector<dory::G1> A;
    std::vector<dory::G2> B;
    for (std::size_t i = 0; i < 4U; ++i) {
        A.push_back(dory::random_g1_for_tests());
        B.push_back(dory::random_g2_for_tests());
    }

    dory::GT expected = dory::gt_one();
    for (std::size_t i = 0; i < A.size(); ++i) {
        expected = dory::gt_mul(expected, dory::pair(A[i], B[i]));
    }

    // PLAN §17.1 equation: pair_product(A,B) = product_i e(A_i,B_i).
    require(dory::gt_equal(dory::pair_product(A, B), expected),
            "pair product identity failed");
}

void test_gt_inverse() {
    const dory::GT X = dory::pair(dory::random_g1_for_tests(),
                                  dory::random_g2_for_tests());
    // PLAN §17.1 equation: X * X^{-1} = 1_GT.
    require(dory::gt_equal(dory::gt_mul(X, dory::gt_inv(X)), dory::gt_one()),
            "GT inverse identity failed");

    // PLAN §2 and §17.1 equation: X^(-1_Fr) = X^{-1} in the order-r GT group.
    const dory::Fr minus_one = dory::neg(dory::one_fr());
    require(dory::gt_equal(dory::gt_pow(X, minus_one), dory::gt_inv(X)),
            "negative scalar GT exponentiation differs from inversion");

    // PLAN §2: arbitrary negative Fr exponents obey X^(-s) = (X^s)^{-1}.
    const dory::Fr s = dory::random_nonzero_fr();
    require(dory::gt_equal(dory::gt_pow(X, dory::neg(s)),
                           dory::gt_inv(dory::gt_pow(X, s))),
            "arbitrary negative scalar GT exponentiation failed");
}

void test_sampling_contracts() {
    require(!dory::g1_equal(dory::g1_generator(), dory::g1_zero()),
            "fixed G1 generator is identity");
    require(!dory::g2_equal(dory::g2_generator(), dory::g2_zero()),
            "fixed G2 generator is identity");

    for (std::size_t i = 0; i < 128U; ++i) {
        require(!dory::is_zero(dory::random_nonzero_fr()),
                "random_nonzero_fr returned zero");
        require(!dory::g1_equal(dory::random_g1_for_tests(), dory::g1_zero()),
                "random test G1 element is identity");
        require(!dory::g2_equal(dory::random_g2_for_tests(), dory::g2_zero()),
                "random test G2 element is identity");
    }
}

void test_pair_product_length_failure() {
    bool threw = false;
    try {
        static_cast<void>(dory::pair_product(
            {dory::g1_generator()},
            {dory::g2_generator(), dory::g2_generator()}));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "pair_product accepted mismatched vector lengths");
}

void test_commitment_equations() {
    const dory::G1 A0 = dory::random_g1_for_tests();
    const dory::G1 A1 = dory::random_g1_for_tests();
    const dory::G2 B0 = dory::random_g2_for_tests();
    const dory::G2 B1 = dory::random_g2_for_tests();
    const dory::Fr r = dory::random_fr();
    const dory::GT Q = dory::pair(dory::g1_generator(), dory::g2_generator());

    // PLAN §6 equation: commit_pairing(A,B,r,Q) = product_i e(A_i,B_i)*Q^r.
    const dory::GT expected_vector = dory::gt_mul(
        dory::gt_mul(dory::pair(A0, B0), dory::pair(A1, B1)),
        dory::gt_pow(Q, r));
    require(dory::gt_equal(dory::commit_pairing({A0, A1}, {B0, B1}, r, Q),
                           expected_vector),
            "vector commitment equation failed");

    // PLAN §6 equation: commit_pairing_scalar(A,B,r,Q) = e(A,B)*Q^r.
    const dory::GT expected_scalar = dory::gt_mul(
        dory::pair(A0, B0), dory::gt_pow(Q, r));
    require(dory::gt_equal(dory::commit_pairing_scalar(A0, B0, r, Q),
                           expected_scalar),
            "scalar commitment equation failed");
}

void test_vector_helpers() {
    require(!dory::is_power_of_two(0U), "zero is not a power of two");
    require(dory::is_power_of_two(1U), "one is a power of two");
    require(dory::is_power_of_two(8U), "eight is a power of two");
    require(!dory::is_power_of_two(6U), "six is not a power of two");
    require(dory::log2_exact(8U) == 3U, "log2_exact(8) must be 3");

    const auto halves = dory::split_half(std::vector<int>{1, 2, 3, 4});
    require(halves.first == std::vector<int>({1, 2}), "left half differs");
    require(halves.second == std::vector<int>({3, 4}), "right half differs");

    const dory::G1 A = dory::random_g1_for_tests();
    const dory::G2 B = dory::random_g2_for_tests();
    const dory::Fr r = dory::random_fr();
    const dory::GT Q = dory::pair(dory::g1_generator(), dory::g2_generator());
    const dory::GT scalar = dory::commit_pairing_scalar(A, B, r, Q);
    const dory::GT vector = dory::commit_pairing({A}, {B}, r, Q);
    require(dory::gt_equal(scalar, vector), "scalar and vector commitments differ");
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_bilinearity();
        test_pair_product();
        test_gt_inverse();
        test_sampling_contracts();
        test_pair_product_length_failure();
        test_commitment_equations();
        test_vector_helpers();
        std::cout << "Milestone 1 backend tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Milestone 1 backend test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
