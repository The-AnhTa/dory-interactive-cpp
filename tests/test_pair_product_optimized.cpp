#include "dory/dory_batch.hpp"
#include "dory/dory_prover.hpp"
#include "dory/dory_verifier.hpp"
#include "dory/scalar_product.hpp"
#include "dory/vector_ops.hpp"

#include <array>
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

class MethodGuard {
public:
    MethodGuard() : previous_(dory::get_pair_product_method()) {}
    ~MethodGuard() { dory::set_pair_product_method(previous_); }
private:
    dory::PairProductMethod previous_;
};

void require_mismatch_rejected(dory::PairProductMethod method) {
    bool threw = false;
    try {
        static_cast<void>(dory::pair_product(
            {dory::g1_generator()}, {}, method));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "pair product accepted mismatched lengths");
}

void test_pair_product_equivalence() {
    constexpr std::array<std::size_t, 10> sizes{
        {0U, 1U, 2U, 3U, 4U, 8U, 16U, 32U, 64U, 128U}};
    for (const std::size_t size : sizes) {
        std::vector<dory::G1> A;
        std::vector<dory::G2> B;
        A.reserve(size);
        B.reserve(size);
        const dory::G1 repeated_g1 = dory::random_g1_for_tests();
        const dory::G2 repeated_g2 = dory::random_g2_for_tests();
        for (std::size_t i = 0; i < size; ++i) {
            A.push_back(i % 5U == 0U ? repeated_g1 : dory::random_g1_for_tests());
            B.push_back(i % 5U == 0U ? repeated_g2 : dory::random_g2_for_tests());
        }
        if (size > 1U) {
            A[1] = dory::g1_zero();
            B[1] = dory::g2_zero();
        }

        require(dory::gt_equal(
                    dory::pair_product_simple(A, B),
                    dory::pair_product_multipairing(A, B)),
                "simple and multi-pairing products differ");

        const dory::Fr r = dory::random_fr();
        const dory::GT Q = dory::pair(
            dory::g1_generator(), dory::g2_generator());
        for (const auto method : {dory::PairProductMethod::Simple,
                                  dory::PairProductMethod::MultiPairing}) {
            dory::set_pair_product_method(method);
            const dory::GT expected = dory::gt_mul(
                dory::pair_product(A, B, method), dory::gt_pow(Q, r));
            require(dory::gt_equal(
                        dory::commit_pairing(A, B, r, Q), expected),
                    "commit_pairing differs for selected pair-product method");
        }
    }

    require(dory::gt_equal(
                dory::pair_product_multipairing({}, {}), dory::gt_one()),
            "empty multi-pairing product is not 1_GT");
    require_mismatch_rejected(dory::PairProductMethod::Simple);
    require_mismatch_rejected(dory::PairProductMethod::MultiPairing);
    require(dory::choose_pair_product_method(1U)
                == dory::PairProductMethod::Simple,
            "default pair-product policy changed below threshold");
    require(dory::choose_pair_product_method(2U)
                == dory::PairProductMethod::MultiPairing,
            "default pair-product policy did not switch at threshold");
}

void test_dory_with_method(dory::PairProductMethod method) {
    dory::set_pair_product_method(method);

    // Milestone 7b: scalar protocol remains valid under either global selection.
    {
        const dory::DoryPrecomp pp = dory::setup_dory_for_tests(1U);
        const auto instance = dory::random_valid_dory_instance(pp);
        dory::ScalarProductStatement stmt{
            instance.first.Gamma[0], instance.first.Lambda[0], instance.first.Q,
            instance.first.D0, instance.first.D1, instance.first.D2};
        dory::ScalarProductWitness wit{
            instance.second.Omega[0], instance.second.Theta[0],
            instance.second.r0, instance.second.r1, instance.second.r2};
        dory::DeterministicChallengeChannel channel(1100000U);
        const auto transcript = dory::prove_scalar_product(stmt, wit, channel);
        require(dory::verify_scalar_product(stmt, transcript),
                "scalar proof failed under pair-product method");
    }

    constexpr std::array<std::size_t, 6> sizes{{1U, 2U, 4U, 8U, 16U, 32U}};
    for (const std::size_t n : sizes) {
        const dory::DoryPrecomp pp = dory::setup_dory_for_tests(n);
        const auto instance = dory::random_valid_dory_instance(pp);
        dory::DeterministicChallengeChannel channel(1200000U + n);
        dory::DoryTranscript transcript = dory::prove_dory_interactive(
            pp, instance.first, instance.second, channel);
        require(dory::verify_dory(pp, instance.first, transcript),
                "recursive proof failed under pair-product method");

        if (n == 8U) {
            transcript.round_msg1[0].D1L = dory::gt_mul(
                transcript.round_msg1[0].D1L,
                dory::pair(dory::g1_generator(), dory::g2_generator()));
            require(!dory::verify_dory(pp, instance.first, transcript),
                    "tampered proof accepted under pair-product method");
        }
    }

    for (const std::size_t n : {8U, 16U}) {
        const dory::DoryPrecomp pp = dory::setup_dory_for_tests(n);
        for (const std::size_t ell : {1U, 2U, 4U}) {
            std::vector<dory::DoryStatement> statements;
            std::vector<dory::DoryWitness> witnesses;
            for (std::size_t i = 0; i < ell; ++i) {
                auto instance = dory::random_valid_dory_instance(pp);
                statements.push_back(std::move(instance.first));
                witnesses.push_back(std::move(instance.second));
            }
            dory::DeterministicChallengeChannel channel(
                1300000U + n * 10U + ell);
            const auto transcript = dory::prove_batch_dory(
                pp, statements, witnesses, channel);
            require(dory::verify_batch_dory(pp, statements, transcript),
                    "batch proof failed under pair-product method");
        }
    }
}

} // namespace

int main() {
    MethodGuard guard;
    try {
        dory::Backend::init_bn254();
        test_pair_product_equivalence();
        test_dory_with_method(dory::PairProductMethod::Simple);
        test_dory_with_method(dory::PairProductMethod::MultiPairing);
        std::cout << "Milestone 7b optimized pair-product tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Optimized pair-product test failure: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
