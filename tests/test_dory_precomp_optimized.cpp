#include "dory/dory_prover.hpp"
#include "dory/dory_verifier.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_equal(
    const dory::DoryPrecomp& simple,
    const dory::DoryPrecomp& optimized) {
    require(simple.n == optimized.n, "precomp n differs");
    require(simple.log_n == optimized.log_n, "precomp log_n differs");
    require(simple.GammaLevels == optimized.GammaLevels, "Gamma levels differ");
    require(simple.LambdaLevels == optimized.LambdaLevels, "Lambda levels differ");
    require(dory::gt_equal(simple.Q, optimized.Q), "precomp Q differs");
    require(simple.levels.size() == optimized.levels.size(), "level count differs");
    for (std::size_t j = 0; j < simple.levels.size(); ++j) {
        require(dory::gt_equal(simple.levels[j].X, optimized.levels[j].X),
                "precomp X differs");
        require(dory::gt_equal(
                    simple.levels[j].Delta1L, optimized.levels[j].Delta1L),
                "precomp Delta1L differs");
        require(dory::gt_equal(
                    simple.levels[j].Delta1R, optimized.levels[j].Delta1R),
                "precomp Delta1R differs");
        require(dory::gt_equal(
                    simple.levels[j].Delta2L, optimized.levels[j].Delta2L),
                "precomp Delta2L differs");
        require(dory::gt_equal(
                    simple.levels[j].Delta2R, optimized.levels[j].Delta2R),
                "precomp Delta2R differs");
    }
}

void test_precomputation_equivalence() {
    constexpr std::array<std::size_t, 7> sizes{{1U, 2U, 4U, 8U, 16U, 32U, 64U}};
    for (const std::size_t n : sizes) {
        const dory::DoryPrecomp generators = dory::setup_dory_for_tests(n);
        dory::PairingProfile simple_profile;
        dory::PairingProfile optimized_profile;
        const dory::DoryPrecomp simple = dory::setup_dory_from_generators_simple(
            generators.GammaLevels[0], generators.LambdaLevels[0], generators.Q,
            &simple_profile, nullptr);
        const dory::DoryPrecomp optimized =
            dory::setup_dory_from_generators_optimized(
                generators.GammaLevels[0], generators.LambdaLevels[0], generators.Q,
                &optimized_profile, nullptr);
        require_equal(simple, optimized);

        const std::size_t expected_simple = n == 1U ? 0U : 6U * n - 6U;
        const std::size_t expected_optimized = n == 1U ? 0U : 3U * n - 2U;
        require(simple_profile.setup_pair_product_terms == expected_simple,
                "simple setup term count differs from 6n-6");
        require(optimized_profile.setup_pair_product_terms == expected_optimized,
                "optimized setup term count differs from 3n-2");

        const auto instance = dory::random_valid_dory_instance(optimized);
        dory::DeterministicChallengeChannel channel(1400000U + n);
        const auto transcript = dory::prove_dory_interactive(
            optimized, instance.first, instance.second, channel);
        require(dory::verify_dory_eager_reference(
                    optimized, instance.first, transcript),
                "eager verifier rejected optimized precomp proof");
        require(dory::verify_dory(optimized, instance.first, transcript),
                "deferred verifier rejected optimized precomp proof");
    }
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_precomputation_equivalence();
        std::cout << "Milestone 7c optimized precomputation tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Optimized precomputation test failure: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

