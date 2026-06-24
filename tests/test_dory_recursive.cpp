#include "dory/dory_prover.hpp"
#include "dory/dory_verifier.hpp"

#include <array>
#include <cstdint>
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

void require_setup_rejected(std::size_t n, const char* message) {
    bool threw = false;
    try {
        static_cast<void>(dory::setup_dory_for_tests(n));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, message);
}

dory::GT nonidentity_gt() {
    return dory::pair(dory::g1_generator(), dory::g2_generator());
}

void require_rejected(
    const dory::DoryPrecomp& pp,
    const dory::DoryStatement& stmt,
    const dory::DoryTranscript& transcript,
    const char* message) {
    require(!dory::verify_dory_eager_reference(pp, stmt, transcript), message);
}

void test_invalid_setup_sizes() {
    require_setup_rejected(0U, "setup accepted n=0");
    require_setup_rejected(3U, "setup accepted non-power-of-two n");

    bool mismatched_threw = false;
    try {
        static_cast<void>(dory::setup_dory_from_generators(
            {dory::hash_to_g1_for_tests("MISMATCH_G1_0")},
            {dory::hash_to_g2_for_tests("MISMATCH_G2_0"),
             dory::hash_to_g2_for_tests("MISMATCH_G2_1")},
            nonidentity_gt()));
    } catch (const std::invalid_argument&) {
        mismatched_threw = true;
    }
    require(mismatched_threw, "setup accepted mismatched Gamma/Lambda sizes");
}

void test_honest_recursive_proofs() {
    constexpr std::array<std::size_t, 6> sizes{{1U, 2U, 4U, 8U, 16U, 32U}};

    for (const std::size_t n : sizes) {
        const dory::DoryPrecomp pp = dory::setup_dory_for_tests(n);
        require(pp.GammaLevels.size() == pp.log_n + 1U,
                "Gamma precomputation level count differs");
        require(pp.LambdaLevels.size() == pp.log_n + 1U,
                "Lambda precomputation level count differs");

        for (std::uint64_t trial = 0; trial < 20U; ++trial) {
            const auto instance = dory::random_valid_dory_instance(pp);
            dory::DeterministicChallengeChannel channel(
                100000U + static_cast<std::uint64_t>(n) * 100U + trial);
            const dory::DoryTranscript transcript = dory::prove_dory_interactive(
                pp, instance.first, instance.second, channel);

            require(dory::verify_dory_eager_reference(pp, instance.first, transcript),
                    "honest recursive Dory proof rejected");

            if (n == 1U) {
                require(transcript.round_msg1.empty()
                            && transcript.round_msg2.empty()
                            && transcript.alpha_challenges.empty()
                            && transcript.beta_challenges.empty(),
                        "n=1 transcript unexpectedly contains recursive rounds");
            }
        }
    }
}

void test_recursive_tampering_rejected() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(8U);
    const auto instance = dory::random_valid_dory_instance(pp);
    dory::DeterministicChallengeChannel channel(777U);
    const dory::DoryTranscript honest = dory::prove_dory_interactive(
        pp, instance.first, instance.second, channel);
    require(dory::verify_dory_eager_reference(pp, instance.first, honest),
            "tamper-test baseline Dory proof rejected");

    const dory::GT delta = nonidentity_gt();
    dory::DoryTranscript transcript = honest;

    transcript.round_msg1[1].D1L = dory::gt_mul(transcript.round_msg1[1].D1L, delta);
    require_rejected(pp, instance.first, transcript, "tampered D1L accepted");

    transcript = honest;
    transcript.round_msg1[1].D1R = dory::gt_mul(transcript.round_msg1[1].D1R, delta);
    require_rejected(pp, instance.first, transcript, "tampered D1R accepted");

    transcript = honest;
    transcript.round_msg1[1].D2L = dory::gt_mul(transcript.round_msg1[1].D2L, delta);
    require_rejected(pp, instance.first, transcript, "tampered D2L accepted");

    transcript = honest;
    transcript.round_msg1[1].D2R = dory::gt_mul(transcript.round_msg1[1].D2R, delta);
    require_rejected(pp, instance.first, transcript, "tampered D2R accepted");

    transcript = honest;
    transcript.round_msg2[1].W1 = dory::gt_mul(transcript.round_msg2[1].W1, delta);
    require_rejected(pp, instance.first, transcript, "tampered W1 accepted");

    transcript = honest;
    transcript.round_msg2[1].W2 = dory::gt_mul(transcript.round_msg2[1].W2, delta);
    require_rejected(pp, instance.first, transcript, "tampered W2 accepted");

    transcript = honest;
    transcript.alpha_challenges[1] =
        transcript.alpha_challenges[1] + dory::one_fr();
    require_rejected(pp, instance.first, transcript, "tampered alpha accepted");

    transcript = honest;
    transcript.beta_challenges[1] =
        transcript.beta_challenges[1] + dory::one_fr();
    require_rejected(pp, instance.first, transcript, "tampered beta accepted");

    transcript = honest;
    transcript.scalar_transcript.msg1.P1 =
        dory::gt_mul(transcript.scalar_transcript.msg1.P1, delta);
    require_rejected(pp, instance.first, transcript, "tampered scalar P1 accepted");

    transcript = honest;
    transcript.scalar_transcript.msg2.E1 = dory::g1_add(
        transcript.scalar_transcript.msg2.E1, dory::g1_generator());
    require_rejected(pp, instance.first, transcript, "tampered scalar E1 accepted");
}

void test_zero_challenges_rejected() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(4U);
    const auto instance = dory::random_valid_dory_instance(pp);
    dory::DeterministicChallengeChannel channel(999U);
    const dory::DoryTranscript honest = dory::prove_dory_interactive(
        pp, instance.first, instance.second, channel);

    dory::DoryTranscript transcript = honest;
    transcript.alpha_challenges[0] = dory::zero_fr();
    require_rejected(pp, instance.first, transcript, "zero recursive alpha accepted");

    transcript = honest;
    transcript.beta_challenges[0] = dory::zero_fr();
    require_rejected(pp, instance.first, transcript, "zero recursive beta accepted");

    transcript = honest;
    transcript.scalar_transcript.epsilon = dory::zero_fr();
    require_rejected(pp, instance.first, transcript, "zero scalar epsilon accepted");

    transcript = honest;
    transcript.scalar_transcript.theta = dory::zero_fr();
    require_rejected(pp, instance.first, transcript, "zero scalar theta accepted");
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_invalid_setup_sizes();
        test_honest_recursive_proofs();
        test_recursive_tampering_rejected();
        test_zero_challenges_rejected();
        std::cout << "Milestone 3 eager recursive Dory tests passed"
                  << " (20 trials for each n=1,2,4,8,16,32)\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Milestone 3 recursive Dory test failure: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
