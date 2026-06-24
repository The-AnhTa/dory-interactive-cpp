#include "dory/dory_prover.hpp"
#include "dory/dory_verifier.hpp"
#include "dory/profiling.hpp"

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

void test_profile_counts() {
    constexpr std::array<std::size_t, 4> sizes{{1U, 2U, 4U, 8U}};
    for (const std::size_t n : sizes) {
        dory::PairingProfile setup_profile;
        dory::TimingProfile setup_timing;
        const dory::DoryPrecomp pp = dory::setup_dory_for_tests(
            n, &setup_profile, &setup_timing);

        dory::PairingProfile instance_profile;
        dory::TimingProfile instance_timing;
        const auto instance = dory::random_valid_dory_instance(
            pp, &instance_profile, &instance_timing);

        // Milestone 7a sanity: D0, D1, D2 each contain n pairing-product terms.
        require(instance_profile.instance_pair_product_calls == 3U,
                "instance profiling did not record three commitments");
        require(instance_profile.instance_pair_product_terms == 3U * n,
                "instance pairing-product terms are not exactly 3n");

        dory::PairingProfile prover_profile;
        dory::TimingProfile prover_timing;
        dory::DeterministicChallengeChannel channel(1000000U + n);
        const dory::DoryTranscript transcript = dory::prove_dory_interactive(
            pp, instance.first, instance.second, channel,
            &prover_profile, &prover_timing);
        require(dory::verify_dory(pp, instance.first, transcript),
                "profiling changed recursive transcript validity");
        require(prover_profile.scalar_pair_calls == 5U,
                "scalar prover pairing count differs from five");

        if (n > 1U) {
            require(prover_profile.prover_d1_pair_product_terms > 0U,
                    "recursive D1 profile terms are zero");
            require(prover_profile.prover_d2_pair_product_terms > 0U,
                    "recursive D2 profile terms are zero");
            require(prover_profile.prover_w_pair_product_terms > 0U,
                    "recursive W profile terms are zero");
        }

        require(setup_timing.setup_ms >= setup_timing.setup_precompute_ms,
                "setup nested timing exceeds total setup timing");
        require(instance_timing.instance_ms >= instance_timing.instance_commit_ms,
                "instance commit timing exceeds total instance timing");
        require(prover_timing.prove_ms >= prover_timing.recursive_prover_ms,
                "recursive prover timing exceeds total proof timing");
    }
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_profile_counts();
        std::cout << "Milestone 7a profile count tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Profile count test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

