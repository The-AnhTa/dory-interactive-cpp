#include "dory/scalar_product.hpp"

#include "dory/vector_ops.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

using Instance = std::pair<dory::ScalarProductStatement, dory::ScalarProductWitness>;

Instance random_valid_instance() {
    dory::ScalarProductWitness wit;
    wit.Omega = dory::random_g1_for_tests();
    wit.Theta = dory::random_g2_for_tests();
    wit.r0 = dory::random_fr();
    wit.r1 = dory::random_fr();
    wit.r2 = dory::random_fr();

    dory::ScalarProductStatement stmt;
    stmt.Gamma = dory::random_g1_for_tests();
    stmt.Lambda = dory::random_g2_for_tests();
    stmt.Q = dory::pair(dory::random_g1_for_tests(),
                        dory::random_g2_for_tests());

    // PLAN §12 relation: D0 = e(Omega, Theta) * Q^r0.
    // Type check: Omega is G1 and Theta is G2.
    stmt.D0 = dory::commit_pairing_scalar(wit.Omega, wit.Theta, wit.r0, stmt.Q);

    // PLAN §12 relation: D1 = e(Omega, Lambda) * Q^r1.
    // Type check: Omega is G1 and Lambda is G2.
    stmt.D1 = dory::commit_pairing_scalar(wit.Omega, stmt.Lambda, wit.r1, stmt.Q);

    // PLAN §12 relation: D2 = e(Gamma, Theta) * Q^r2.
    // Type check: Gamma is G1 and Theta is G2.
    stmt.D2 = dory::commit_pairing_scalar(stmt.Gamma, wit.Theta, wit.r2, stmt.Q);

    return {stmt, wit};
}

dory::GT nonidentity_gt() {
    return dory::pair(dory::g1_generator(), dory::g2_generator());
}

void require_rejected(
    const dory::ScalarProductStatement& stmt,
    const dory::ScalarProductTranscript& transcript,
    const char* message) {
    require(!dory::verify_scalar_product(stmt, transcript), message);
}

void test_type_correctness() {
    using PairFunction = decltype(&dory::pair);

    // PLAN §12.1 type check: the backend admits only G1 x G2 -> GT.
    static_assert(std::is_invocable_r_v<dory::GT, PairFunction,
                                        const dory::G1&, const dory::G2&>);
    static_assert(!std::is_invocable_v<PairFunction,
                                       const dory::G1&, const dory::G1&>);
    static_assert(!std::is_invocable_v<PairFunction,
                                       const dory::G2&, const dory::G2&>);
    static_assert(!std::is_invocable_v<PairFunction,
                                       const dory::G2&, const dory::G1&>);
    static_assert(std::is_same_v<decltype(dory::ScalarProductMsg2::E1), dory::G1>);
    static_assert(std::is_same_v<decltype(dory::ScalarProductMsg2::E2), dory::G2>);
}

void test_fifty_honest_trials() {
    for (std::uint64_t trial = 0; trial < 50U; ++trial) {
        const Instance instance = random_valid_instance();
        dory::DeterministicChallengeChannel channel(1000U + trial);
        const dory::ScalarProductTranscript transcript = dory::prove_scalar_product(
            instance.first, instance.second, channel);
        require(dory::verify_scalar_product(instance.first, transcript),
                "honest scalar-product proof rejected");
    }
}

void test_all_tampering_rejected() {
    const Instance instance = random_valid_instance();
    dory::DeterministicChallengeChannel channel(424242U);
    const dory::ScalarProductTranscript honest = dory::prove_scalar_product(
        instance.first, instance.second, channel);
    require(dory::verify_scalar_product(instance.first, honest),
            "tamper-test baseline proof rejected");

    const dory::GT delta = nonidentity_gt();
    dory::ScalarProductTranscript transcript = honest;

    transcript.msg1.P1 = dory::gt_mul(transcript.msg1.P1, delta);
    require_rejected(instance.first, transcript, "tampered P1 accepted");

    transcript = honest;
    transcript.msg1.P2 = dory::gt_mul(transcript.msg1.P2, delta);
    require_rejected(instance.first, transcript, "tampered P2 accepted");

    transcript = honest;
    transcript.msg1.S = dory::gt_mul(transcript.msg1.S, delta);
    require_rejected(instance.first, transcript, "tampered S accepted");

    transcript = honest;
    transcript.msg1.R = dory::gt_mul(transcript.msg1.R, delta);
    require_rejected(instance.first, transcript, "tampered R accepted");

    transcript = honest;
    transcript.msg2.E1 = dory::g1_add(transcript.msg2.E1, dory::g1_generator());
    require_rejected(instance.first, transcript, "tampered E1 accepted");

    transcript = honest;
    transcript.msg2.E2 = dory::g2_add(transcript.msg2.E2, dory::g2_generator());
    require_rejected(instance.first, transcript, "tampered E2 accepted");

    transcript = honest;
    transcript.msg2.r1_resp = transcript.msg2.r1_resp + dory::one_fr();
    require_rejected(instance.first, transcript, "tampered r1_resp accepted");

    transcript = honest;
    transcript.msg2.r2_resp = transcript.msg2.r2_resp + dory::one_fr();
    require_rejected(instance.first, transcript, "tampered r2_resp accepted");

    transcript = honest;
    transcript.msg2.r0_hat = transcript.msg2.r0_hat + dory::one_fr();
    require_rejected(instance.first, transcript, "tampered r0_hat accepted");

    dory::ScalarProductStatement stmt = instance.first;
    stmt.D0 = dory::gt_mul(stmt.D0, delta);
    require_rejected(stmt, honest, "tampered D0 accepted");

    stmt = instance.first;
    stmt.D1 = dory::gt_mul(stmt.D1, delta);
    require_rejected(stmt, honest, "tampered D1 accepted");

    stmt = instance.first;
    stmt.D2 = dory::gt_mul(stmt.D2, delta);
    require_rejected(stmt, honest, "tampered D2 accepted");
}

void test_zero_challenges_rejected() {
    const Instance instance = random_valid_instance();
    dory::RandomChallengeChannel channel;
    const dory::ScalarProductTranscript honest = dory::prove_scalar_product(
        instance.first, instance.second, channel);

    dory::ScalarProductTranscript transcript = honest;
    transcript.epsilon = dory::zero_fr();
    require_rejected(instance.first, transcript, "zero epsilon accepted");

    transcript = honest;
    transcript.theta = dory::zero_fr();
    require_rejected(instance.first, transcript, "zero theta accepted");
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_type_correctness();
        test_fifty_honest_trials();
        test_all_tampering_rejected();
        test_zero_challenges_rejected();
        std::cout << "Milestone 2 scalar-product tests passed (50 honest trials)\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Milestone 2 scalar-product test failure: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

