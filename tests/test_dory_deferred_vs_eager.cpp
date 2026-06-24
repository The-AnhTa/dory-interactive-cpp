#include "dory/dory_prover.hpp"
#include "dory/dory_verifier.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

dory::GT nonidentity_gt() {
    return dory::pair(dory::g1_generator(), dory::g2_generator());
}

void require_agree_and_reject(
    const dory::DoryPrecomp& pp,
    const dory::DoryStatement& stmt,
    const dory::DoryTranscript& transcript,
    const char* message) {
    const bool eager = dory::verify_dory_eager_reference(pp, stmt, transcript);
    const bool simple = dory::verify_dory_deferred_multiexp(
        pp, stmt, transcript, nullptr, dory::GtMultiexpMethod::Simple);
    const bool windowed = dory::verify_dory_deferred_multiexp(
        pp, stmt, transcript, nullptr, dory::GtMultiexpMethod::WindowedBucket);
    require(eager == simple, "eager/simple tamper verdicts differ");
    require(eager == windowed, "eager/windowed tamper verdicts differ");
    require(!eager, message);
}

dory::DoryStatement eager_final_statement(
    const dory::DoryPrecomp& pp,
    const dory::DoryStatement& stmt,
    const dory::DoryTranscript& transcript) {
    dory::DoryStatement cur = stmt;
    for (std::size_t j = 0; j < pp.log_n; ++j) {
        // PLAN §11 and §14 equivalence oracle: explicitly evaluate the eager
        // recurrence against the same transcript and public precomputation.
        cur = dory::fold_statement_eager(
            cur,
            transcript.round_msg1[j],
            transcript.beta_challenges[j],
            transcript.round_msg2[j],
            transcript.alpha_challenges[j],
            pp.levels[j],
            pp.GammaLevels[j + 1U],
            pp.LambdaLevels[j + 1U]);
    }
    return cur;
}

void test_honest_equivalence() {
    constexpr std::array<std::size_t, 6> sizes{{1U, 2U, 4U, 8U, 16U, 32U}};

    for (const std::size_t n : sizes) {
        const dory::DoryPrecomp pp = dory::setup_dory_for_tests(n);
        for (std::uint64_t trial = 0; trial < 20U; ++trial) {
            const auto instance = dory::random_valid_dory_instance(pp);
            dory::DeterministicChallengeChannel channel(
                200000U + static_cast<std::uint64_t>(n) * 100U + trial);
            const dory::DoryTranscript transcript = dory::prove_dory_interactive(
                pp, instance.first, instance.second, channel);

            const bool eager = dory::verify_dory_eager_reference(
                pp, instance.first, transcript);
            const bool deferred = dory::verify_dory_deferred_multiexp(
                pp, instance.first, transcript);
            const bool simple = dory::verify_dory_deferred_multiexp(
                pp, instance.first, transcript, nullptr,
                dory::GtMultiexpMethod::Simple);
            const bool windowed = dory::verify_dory_deferred_multiexp(
                pp, instance.first, transcript, nullptr,
                dory::GtMultiexpMethod::WindowedBucket);
            require(eager, "eager verifier rejected honest transcript");
            require(deferred, "deferred verifier rejected honest transcript");
            require(simple, "simple deferred verifier rejected honest transcript");
            require(windowed, "windowed deferred verifier rejected honest transcript");
            require(eager == deferred, "honest eager/deferred verdicts differ");
            require(eager == simple, "honest eager/simple verdicts differ");
            require(eager == windowed, "honest eager/windowed verdicts differ");
            require(dory::verify_dory(pp, instance.first, transcript) == deferred,
                    "default verifier does not select deferred verifier");
        }
    }
}

void test_symbolic_state_matches_eager_fold() {
    constexpr std::array<std::size_t, 6> sizes{{1U, 2U, 4U, 8U, 16U, 32U}};

    for (const std::size_t n : sizes) {
        const dory::DoryPrecomp pp = dory::setup_dory_for_tests(n);
        for (std::uint64_t trial = 0; trial < 5U; ++trial) {
            const auto instance = dory::random_valid_dory_instance(pp);
            dory::DeterministicChallengeChannel channel(
                300000U + static_cast<std::uint64_t>(n) * 100U + trial);
            const dory::DoryTranscript transcript = dory::prove_dory_interactive(
                pp, instance.first, instance.second, channel);

            dory::DeferredDoryState state;
            require(dory::build_dory_deferred_state(
                        pp, instance.first, transcript, state),
                    "could not build deferred symbolic state");
            const dory::DoryStatement eager_final = eager_final_statement(
                pp, instance.first, transcript);

            // PLAN §14.2-§14.3 key sanity check: eval(A0) = eager final D0.
            require(dory::gt_equal(dory::eval_gt_multiexp(state.A0), eager_final.D0),
                    "eval(A0) differs from eager final D0");

            // PLAN §14.2-§14.3 key sanity check: eval(A1) = eager final D1.
            require(dory::gt_equal(dory::eval_gt_multiexp(state.A1), eager_final.D1),
                    "eval(A1) differs from eager final D1");

            // PLAN §14.2-§14.3 key sanity check: eval(A2) = eager final D2.
            require(dory::gt_equal(dory::eval_gt_multiexp(state.A2), eager_final.D2),
                    "eval(A2) differs from eager final D2");
        }
    }
}

void test_tampered_equivalence() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(8U);
    const auto instance = dory::random_valid_dory_instance(pp);
    dory::DeterministicChallengeChannel channel(808080U);
    const dory::DoryTranscript honest = dory::prove_dory_interactive(
        pp, instance.first, instance.second, channel);
    require(dory::verify_dory_eager_reference(pp, instance.first, honest),
            "tamper baseline eager verification failed");
    require(dory::verify_dory_deferred_multiexp(pp, instance.first, honest),
            "tamper baseline deferred verification failed");

    const dory::GT delta = nonidentity_gt();
    dory::DoryTranscript tr = honest;

    tr.round_msg1[1].D1L = dory::gt_mul(tr.round_msg1[1].D1L, delta);
    require_agree_and_reject(pp, instance.first, tr, "tampered D1L accepted");

    tr = honest;
    tr.round_msg1[1].D1R = dory::gt_mul(tr.round_msg1[1].D1R, delta);
    require_agree_and_reject(pp, instance.first, tr, "tampered D1R accepted");

    tr = honest;
    tr.round_msg1[1].D2L = dory::gt_mul(tr.round_msg1[1].D2L, delta);
    require_agree_and_reject(pp, instance.first, tr, "tampered D2L accepted");

    tr = honest;
    tr.round_msg1[1].D2R = dory::gt_mul(tr.round_msg1[1].D2R, delta);
    require_agree_and_reject(pp, instance.first, tr, "tampered D2R accepted");

    tr = honest;
    tr.round_msg2[1].W1 = dory::gt_mul(tr.round_msg2[1].W1, delta);
    require_agree_and_reject(pp, instance.first, tr, "tampered W1 accepted");

    tr = honest;
    tr.round_msg2[1].W2 = dory::gt_mul(tr.round_msg2[1].W2, delta);
    require_agree_and_reject(pp, instance.first, tr, "tampered W2 accepted");

    tr = honest;
    tr.alpha_challenges[1] = tr.alpha_challenges[1] + dory::one_fr();
    require_agree_and_reject(pp, instance.first, tr, "tampered alpha accepted");

    tr = honest;
    tr.beta_challenges[1] = tr.beta_challenges[1] + dory::one_fr();
    require_agree_and_reject(pp, instance.first, tr, "tampered beta accepted");

    tr = honest;
    tr.scalar_transcript.msg1.P1 = dory::gt_mul(
        tr.scalar_transcript.msg1.P1, delta);
    require_agree_and_reject(pp, instance.first, tr, "tampered P1 accepted");

    tr = honest;
    tr.scalar_transcript.msg1.P2 = dory::gt_mul(
        tr.scalar_transcript.msg1.P2, delta);
    require_agree_and_reject(pp, instance.first, tr, "tampered P2 accepted");

    tr = honest;
    tr.scalar_transcript.msg1.S = dory::gt_mul(
        tr.scalar_transcript.msg1.S, delta);
    require_agree_and_reject(pp, instance.first, tr, "tampered S accepted");

    tr = honest;
    tr.scalar_transcript.msg1.R = dory::gt_mul(
        tr.scalar_transcript.msg1.R, delta);
    require_agree_and_reject(pp, instance.first, tr, "tampered R accepted");

    tr = honest;
    tr.scalar_transcript.msg2.E1 = dory::g1_add(
        tr.scalar_transcript.msg2.E1, dory::g1_generator());
    require_agree_and_reject(pp, instance.first, tr, "tampered E1 accepted");

    tr = honest;
    tr.scalar_transcript.msg2.E2 = dory::g2_add(
        tr.scalar_transcript.msg2.E2, dory::g2_generator());
    require_agree_and_reject(pp, instance.first, tr, "tampered E2 accepted");

    tr = honest;
    tr.scalar_transcript.msg2.r0_hat =
        tr.scalar_transcript.msg2.r0_hat + dory::one_fr();
    require_agree_and_reject(pp, instance.first, tr, "tampered r0_hat accepted");

    tr = honest;
    tr.scalar_transcript.msg2.r1_resp =
        tr.scalar_transcript.msg2.r1_resp + dory::one_fr();
    require_agree_and_reject(pp, instance.first, tr, "tampered r1_resp accepted");

    tr = honest;
    tr.scalar_transcript.msg2.r2_resp =
        tr.scalar_transcript.msg2.r2_resp + dory::one_fr();
    require_agree_and_reject(pp, instance.first, tr, "tampered r2_resp accepted");
}

void test_zero_challenge_equivalence() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(8U);
    const auto instance = dory::random_valid_dory_instance(pp);
    dory::DeterministicChallengeChannel channel(909090U);
    const dory::DoryTranscript honest = dory::prove_dory_interactive(
        pp, instance.first, instance.second, channel);

    dory::DoryTranscript tr = honest;
    tr.alpha_challenges[0] = dory::zero_fr();
    require_agree_and_reject(pp, instance.first, tr, "zero alpha accepted");

    tr = honest;
    tr.beta_challenges[0] = dory::zero_fr();
    require_agree_and_reject(pp, instance.first, tr, "zero beta accepted");

    tr = honest;
    tr.scalar_transcript.epsilon = dory::zero_fr();
    require_agree_and_reject(pp, instance.first, tr, "zero epsilon accepted");

    tr = honest;
    tr.scalar_transcript.theta = dory::zero_fr();
    require_agree_and_reject(pp, instance.first, tr, "zero theta accepted");
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_honest_equivalence();
        test_symbolic_state_matches_eager_fold();
        test_tampered_equivalence();
        test_zero_challenge_equivalence();
        std::cout << "Milestone 4 deferred/eager tests passed"
                  << " (120 honest equivalence trials)\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Milestone 4 deferred/eager test failure: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
