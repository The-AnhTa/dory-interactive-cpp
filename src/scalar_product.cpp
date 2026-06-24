#include "dory/scalar_product.hpp"

#include "dory/vector_ops.hpp"

#include <chrono>
#include <stdexcept>

namespace dory {

ScalarProductTranscript prove_scalar_product(
    const ScalarProductStatement& stmt,
    const ScalarProductWitness& wit,
    ChallengeChannel& channel) {
    return prove_scalar_product(stmt, wit, channel, nullptr, nullptr);
}

ScalarProductTranscript prove_scalar_product(
    const ScalarProductStatement& stmt,
    const ScalarProductWitness& wit,
    ChallengeChannel& channel,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile) {
    const auto scalar_start = std::chrono::steady_clock::now();
    // PLAN §12.1: OmegaRand <- G1.
    // Additive implementation: sample a nonzero scalar times the fixed G1 point.
    const G1 OmegaRand = g1_mul(g1_generator(), random_nonzero_fr());

    // PLAN §12.1: ThetaRand <- G2.
    // Additive implementation: sample a nonzero scalar times the fixed G2 point.
    const G2 ThetaRand = g2_mul(g2_generator(), random_nonzero_fr());

    // PLAN §12.1: rP1, rP2, rS, rR <- Fr.
    const Fr rP1 = random_fr();
    const Fr rP2 = random_fr();
    const Fr rS = random_fr();
    const Fr rR = random_fr();

    ScalarProductTranscript transcript;

    // PLAN §12.1: P1 = e(OmegaRand, Lambda) * Q^rP1.
    // Type check: OmegaRand is G1 and Lambda is G2.
    transcript.msg1.P1 = gt_mul(
        pair_profiled(
            OmegaRand, stmt.Lambda, pairing_profile, PairingProfileLabel::Scalar),
        gt_pow(stmt.Q, rP1));

    // PLAN §12.1: P2 = e(Gamma, ThetaRand) * Q^rP2.
    // Type check: Gamma is G1 and ThetaRand is G2.
    transcript.msg1.P2 = gt_mul(
        pair_profiled(
            stmt.Gamma, ThetaRand, pairing_profile, PairingProfileLabel::Scalar),
        gt_pow(stmt.Q, rP2));

    // PLAN §12.1: S = e(OmegaRand, Theta) * e(Omega, ThetaRand) * Q^rS.
    // Type checks: both pairing calls have a G1 first and a G2 second.
    transcript.msg1.S = gt_mul(
        gt_mul(
            pair_profiled(
                OmegaRand, wit.Theta, pairing_profile, PairingProfileLabel::Scalar),
            pair_profiled(
                wit.Omega, ThetaRand, pairing_profile, PairingProfileLabel::Scalar)),
        gt_pow(stmt.Q, rS));

    // PLAN §12.1: R = e(OmegaRand, ThetaRand) * Q^rR.
    // Type check: OmegaRand is G1 and ThetaRand is G2.
    transcript.msg1.R = gt_mul(
        pair_profiled(
            OmegaRand, ThetaRand, pairing_profile, PairingProfileLabel::Scalar),
        gt_pow(stmt.Q, rR));

    // PLAN §12.2: epsilon <- Fr* after the first prover message.
    transcript.epsilon = channel.get_scalar_epsilon();
    if (is_zero(transcript.epsilon)) {
        throw std::invalid_argument("scalar epsilon challenge must be nonzero");
    }

    // PLAN §12.2: E1 = OmegaRand + epsilon * Omega.
    // Additive translation: paper source-group multiplication becomes G1 addition.
    transcript.msg2.E1 = g1_add(
        OmegaRand, g1_mul(wit.Omega, transcript.epsilon));

    // PLAN §12.2: E2 = ThetaRand + epsilon * Theta.
    // Additive translation: paper source-group multiplication becomes G2 addition.
    transcript.msg2.E2 = g2_add(
        ThetaRand, g2_mul(wit.Theta, transcript.epsilon));

    // PLAN §12.2: r1_resp = rP1 + epsilon * r1.
    transcript.msg2.r1_resp = rP1 + transcript.epsilon * wit.r1;

    // PLAN §12.2: r2_resp = rP2 + epsilon * r2.
    transcript.msg2.r2_resp = rP2 + transcript.epsilon * wit.r2;

    // PLAN §12.2: r0_hat = rR + epsilon * rS + epsilon^2 * r0.
    transcript.msg2.r0_hat = rR
                           + transcript.epsilon * rS
                           + transcript.epsilon * transcript.epsilon * wit.r0;

    // PLAN §12.3: theta <- Fr* after the second prover message.
    transcript.theta = channel.get_scalar_theta();
    if (is_zero(transcript.theta)) {
        throw std::invalid_argument("scalar theta challenge must be nonzero");
    }

    if (timing_profile != nullptr) {
        timing_profile->scalar_prover_ms +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - scalar_start).count();
    }
    return transcript;
}

bool verify_scalar_product(
    const ScalarProductStatement& stmt,
    const ScalarProductTranscript& transcript) {
    const Fr& epsilon = transcript.epsilon;
    const Fr& theta = transcript.theta;

    // PLAN §9 and §12.3: epsilon and theta are required nonzero challenges.
    // Reject malformed transcripts before attempting theta inversion.
    if (is_zero(epsilon) || is_zero(theta)) {
        return false;
    }

    // PLAN §12.3: theta_inv = theta^{-1}.
    const Fr theta_inv = inv(theta);

    // PLAN §12.3: r = r0_hat + theta*r2_resp + theta_inv*r1_resp.
    const Fr r = transcript.msg2.r0_hat
               + theta * transcript.msg2.r2_resp
               + theta_inv * transcript.msg2.r1_resp;

    // PLAN §12.3: lhs_G1 = E1 + theta * Gamma.
    // Additive translation: theta * Gamma is scalar multiplication in G1.
    const G1 lhs_g1 = g1_add(
        transcript.msg2.E1, g1_mul(stmt.Gamma, theta));

    // PLAN §12.3: lhs_G2 = E2 + theta_inv * Lambda.
    // Additive translation: theta_inv * Lambda is scalar multiplication in G2.
    const G2 lhs_g2 = g2_add(
        transcript.msg2.E2, g2_mul(stmt.Lambda, theta_inv));

    // PLAN §12.3: lhs = e(E1 + theta*Gamma, E2 + theta_inv*Lambda).
    // Type check: lhs_g1 is G1 and lhs_g2 is G2.
    const GT lhs = pair(lhs_g1, lhs_g2);

    // PLAN §12.3: rhs begins with e(Gamma, Lambda).
    // Type check: Gamma is G1 and Lambda is G2.
    GT rhs = pair(stmt.Gamma, stmt.Lambda);

    // PLAN §12.3: rhs *= R.
    rhs = gt_mul(rhs, transcript.msg1.R);

    // PLAN §12.3: rhs *= S^epsilon.
    rhs = gt_mul(rhs, gt_pow(transcript.msg1.S, epsilon));

    // PLAN §12.3: rhs *= D0^(epsilon^2).
    rhs = gt_mul(rhs, gt_pow(stmt.D0, epsilon * epsilon));

    // PLAN §12.3: rhs *= P2^theta.
    rhs = gt_mul(rhs, gt_pow(transcript.msg1.P2, theta));

    // PLAN §12.3: rhs *= D2^(theta * epsilon).
    rhs = gt_mul(rhs, gt_pow(stmt.D2, theta * epsilon));

    // PLAN §12.3: rhs *= P1^(theta_inv).
    rhs = gt_mul(rhs, gt_pow(transcript.msg1.P1, theta_inv));

    // PLAN §12.3: rhs *= D1^(theta_inv * epsilon).
    rhs = gt_mul(rhs, gt_pow(stmt.D1, theta_inv * epsilon));

    // PLAN §12.3: rhs *= Q^(-r), not Q^(+r).
    // GT translation: the negative Fr exponent removes commitment blindings.
    rhs = gt_mul(rhs, gt_pow(stmt.Q, neg(r)));

    return gt_equal(lhs, rhs);
}

} // namespace dory
