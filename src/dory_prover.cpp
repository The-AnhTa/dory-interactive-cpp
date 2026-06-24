#include "dory/dory_prover.hpp"

#include "dory/scalar_product.hpp"
#include "dory/vector_ops.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dory {
namespace {

template <typename T>
bool vectors_equal(const std::vector<T>& a, const std::vector<T>& b) {
    return a == b;
}

} // namespace

DoryStatement fold_statement_eager(
    const DoryStatement& cur,
    const DoryRoundMsg1& msg1,
    const Fr& beta,
    const DoryRoundMsg2& msg2,
    const Fr& alpha,
    const DoryLevelPrecomp& pc,
    const std::vector<G1>& GammaPrime,
    const std::vector<G2>& LambdaPrime) {
    if (is_zero(beta) || is_zero(alpha)) {
        throw std::invalid_argument("Dory fold challenges must be nonzero");
    }
    if (cur.n < 2U || (cur.n & 1U) != 0U
        || GammaPrime.size() != cur.n / 2U
        || LambdaPrime.size() != cur.n / 2U) {
        throw std::invalid_argument("invalid Dory fold dimensions");
    }

    // PLAN §11: beta_inv = beta^{-1}.
    const Fr beta_inv = inv(beta);

    // PLAN §11: alpha_inv = alpha^{-1}.
    const Fr alpha_inv = inv(alpha);

    // PLAN §11 equation: D0Prime starts at D0 * X.
    GT D0Prime = gt_mul(cur.D0, pc.X);

    // PLAN §11 equation: D0Prime *= D1^(beta_inv).
    D0Prime = gt_mul(D0Prime, gt_pow(cur.D1, beta_inv));

    // PLAN §11 equation: D0Prime *= D2^beta.
    D0Prime = gt_mul(D0Prime, gt_pow(cur.D2, beta));

    // PLAN §11 equation: D0Prime *= W1^alpha.
    D0Prime = gt_mul(D0Prime, gt_pow(msg2.W1, alpha));

    // PLAN §11 equation: D0Prime *= W2^(alpha_inv).
    D0Prime = gt_mul(D0Prime, gt_pow(msg2.W2, alpha_inv));

    // PLAN §11 equation: D1Prime starts at D1L^alpha * D1R.
    GT D1Prime = gt_mul(gt_pow(msg1.D1L, alpha), msg1.D1R);

    // PLAN §11 equation: D1Prime *= Delta1L^(alpha * beta).
    D1Prime = gt_mul(D1Prime, gt_pow(pc.Delta1L, alpha * beta));

    // PLAN §11 equation: D1Prime *= Delta1R^beta.
    D1Prime = gt_mul(D1Prime, gt_pow(pc.Delta1R, beta));

    // PLAN §11 equation: D2Prime starts at D2L^(alpha_inv) * D2R.
    GT D2Prime = gt_mul(gt_pow(msg1.D2L, alpha_inv), msg1.D2R);

    // PLAN §11 equation: D2Prime *= Delta2L^(alpha_inv * beta_inv).
    D2Prime = gt_mul(
        D2Prime, gt_pow(pc.Delta2L, alpha_inv * beta_inv));

    // PLAN §11 equation: D2Prime *= Delta2R^(beta_inv).
    D2Prime = gt_mul(D2Prime, gt_pow(pc.Delta2R, beta_inv));

    DoryStatement next;
    // PLAN §11: n' = n/2, Gamma' = GammaPrime, Lambda' = LambdaPrime.
    next.n = cur.n / 2U;
    next.Gamma = GammaPrime;
    next.Lambda = LambdaPrime;
    // PLAN §11: Q is unchanged by recursive folding.
    next.Q = cur.Q;
    next.D0 = D0Prime;
    next.D1 = D1Prime;
    next.D2 = D2Prime;
    return next;
}

DoryTranscript prove_dory_interactive(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryWitness& wit,
    ChallengeChannel& channel) {
    return prove_dory_interactive(
        pp, stmt, wit, channel, nullptr, nullptr);
}

DoryTranscript prove_dory_interactive(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryWitness& wit,
    ChallengeChannel& channel,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile) {
    const auto prove_start = std::chrono::steady_clock::now();
    if (stmt.n != pp.n || stmt.n == 0U || !is_power_of_two(stmt.n)
        || stmt.Gamma.size() != stmt.n || stmt.Lambda.size() != stmt.n
        || wit.Omega.size() != stmt.n || wit.Theta.size() != stmt.n
        || pp.GammaLevels.size() != pp.log_n + 1U
        || pp.LambdaLevels.size() != pp.log_n + 1U
        || pp.levels.size() != pp.log_n
        || !vectors_equal(stmt.Gamma, pp.GammaLevels[0])
        || !vectors_equal(stmt.Lambda, pp.LambdaLevels[0])
        || !gt_equal(stmt.Q, pp.Q)) {
        throw std::invalid_argument("statement, witness, and precomputation differ");
    }

    DoryTranscript transcript;
    transcript.round_msg1.reserve(pp.log_n);
    transcript.beta_challenges.reserve(pp.log_n);
    transcript.round_msg2.reserve(pp.log_n);
    transcript.alpha_challenges.reserve(pp.log_n);

    DoryStatement cur = stmt;
    std::vector<G1> Omega = wit.Omega;
    std::vector<G2> Theta = wit.Theta;
    Fr r0 = wit.r0;
    Fr r1 = wit.r1;
    Fr r2 = wit.r2;

    const auto recursive_start = std::chrono::steady_clock::now();
    for (std::size_t j = 0; j < pp.log_n; ++j) {
        const std::vector<G1>& Gamma = cur.Gamma;
        const std::vector<G2>& Lambda = cur.Lambda;
        const std::vector<G1>& GammaPrime = pp.GammaLevels[j + 1U];
        const std::vector<G2>& LambdaPrime = pp.LambdaLevels[j + 1U];

        // PLAN §10.1: Omega = OmegaL || OmegaR.
        const auto omega_halves = split_half(Omega);

        // PLAN §10.1: Theta = ThetaL || ThetaR.
        const auto theta_halves = split_half(Theta);

        // PLAN §10.1: r1L, r1R, r2L, r2R <- Fr.
        const Fr r1L = random_fr();
        const Fr r1R = random_fr();
        const Fr r2L = random_fr();
        const Fr r2R = random_fr();

        DoryRoundMsg1 msg1;

        // PLAN §10.1: D1L = e(OmegaL, LambdaPrime) * Q^r1L.
        msg1.D1L = gt_mul(
            pair_product_profiled(
                omega_halves.first, LambdaPrime, pairing_profile,
                PairingProfileLabel::ProverD1),
            gt_pow(cur.Q, r1L));

        // PLAN §10.1: D1R = e(OmegaR, LambdaPrime) * Q^r1R.
        msg1.D1R = gt_mul(
            pair_product_profiled(
                omega_halves.second, LambdaPrime, pairing_profile,
                PairingProfileLabel::ProverD1),
            gt_pow(cur.Q, r1R));

        // PLAN §10.1: D2L = e(GammaPrime, ThetaL) * Q^r2L.
        msg1.D2L = gt_mul(
            pair_product_profiled(
                GammaPrime, theta_halves.first, pairing_profile,
                PairingProfileLabel::ProverD2),
            gt_pow(cur.Q, r2L));

        // PLAN §10.1: D2R = e(GammaPrime, ThetaR) * Q^r2R.
        msg1.D2R = gt_mul(
            pair_product_profiled(
                GammaPrime, theta_halves.second, pairing_profile,
                PairingProfileLabel::ProverD2),
            gt_pow(cur.Q, r2R));

        transcript.round_msg1.push_back(msg1);

        // PLAN §10.2: beta <- Fr* after the first round message.
        const Fr beta = channel.get_beta(j);
        if (is_zero(beta)) {
            throw std::invalid_argument("recursive beta challenge must be nonzero");
        }
        transcript.beta_challenges.push_back(beta);

        // PLAN §10.2: beta_inv = beta^{-1}.
        const Fr beta_inv = inv(beta);

        std::vector<G1> OmegaCirc;
        std::vector<G2> ThetaCirc;
        OmegaCirc.reserve(cur.n);
        ThetaCirc.reserve(cur.n);
        for (std::size_t i = 0; i < cur.n; ++i) {
            // PLAN §10.2: OmegaCirc[i] = Omega[i] + beta * Gamma[i].
            // Additive translation: Gamma[i]^beta becomes beta * Gamma[i].
            OmegaCirc.push_back(g1_add(Omega[i], g1_mul(Gamma[i], beta)));

            // PLAN §10.2: ThetaCirc[i] = Theta[i] + beta_inv * Lambda[i].
            // Additive translation: Lambda[i]^beta_inv becomes scalar multiplication.
            ThetaCirc.push_back(g2_add(Theta[i], g2_mul(Lambda[i], beta_inv)));
        }

        // PLAN §10.2: r0_tilde = r0 + beta_inv*r1 + beta*r2.
        const Fr r0_tilde = r0 + beta_inv * r1 + beta * r2;

        // PLAN §10.3: OmegaCirc = OmegaCircL || OmegaCircR.
        const auto omega_circ_halves = split_half(OmegaCirc);

        // PLAN §10.3: ThetaCirc = ThetaCircL || ThetaCircR.
        const auto theta_circ_halves = split_half(ThetaCirc);

        // PLAN §10.3: rW1, rW2 <- Fr.
        const Fr rW1 = random_fr();
        const Fr rW2 = random_fr();

        DoryRoundMsg2 msg2;

        // PLAN §10.3: W1 = e(OmegaCircL, ThetaCircR) * Q^rW1.
        msg2.W1 = gt_mul(
            pair_product_profiled(
                omega_circ_halves.first, theta_circ_halves.second,
                pairing_profile, PairingProfileLabel::ProverW),
            gt_pow(cur.Q, rW1));

        // PLAN §10.3: W2 = e(OmegaCircR, ThetaCircL) * Q^rW2.
        msg2.W2 = gt_mul(
            pair_product_profiled(
                omega_circ_halves.second, theta_circ_halves.first,
                pairing_profile, PairingProfileLabel::ProverW),
            gt_pow(cur.Q, rW2));

        transcript.round_msg2.push_back(msg2);

        // PLAN §10.4: alpha <- Fr* after the second round message.
        const Fr alpha = channel.get_alpha(j);
        if (is_zero(alpha)) {
            throw std::invalid_argument("recursive alpha challenge must be nonzero");
        }
        transcript.alpha_challenges.push_back(alpha);

        // PLAN §10.4: alpha_inv = alpha^{-1}.
        const Fr alpha_inv = inv(alpha);

        std::vector<G1> OmegaPrime;
        std::vector<G2> ThetaPrime;
        OmegaPrime.reserve(cur.n / 2U);
        ThetaPrime.reserve(cur.n / 2U);
        for (std::size_t i = 0; i < cur.n / 2U; ++i) {
            // PLAN §10.4: OmegaPrime[i] = alpha*OmegaCircL[i] + OmegaCircR[i].
            OmegaPrime.push_back(g1_add(
                g1_mul(omega_circ_halves.first[i], alpha),
                omega_circ_halves.second[i]));

            // PLAN §10.4: ThetaPrime[i] = alpha_inv*ThetaCircL[i] + ThetaCircR[i].
            ThetaPrime.push_back(g2_add(
                g2_mul(theta_circ_halves.first[i], alpha_inv),
                theta_circ_halves.second[i]));
        }

        // PLAN §10.4: r0Prime = r0_tilde + alpha*rW1 + alpha_inv*rW2.
        const Fr r0Prime = r0_tilde + alpha * rW1 + alpha_inv * rW2;

        // PLAN §10.4: r1Prime = alpha*r1L + r1R.
        const Fr r1Prime = alpha * r1L + r1R;

        // PLAN §10.4: r2Prime = alpha_inv*r2L + r2R.
        const Fr r2Prime = alpha_inv * r2L + r2R;

        // PLAN §10.4 and §11: synchronize the prover's public statement using
        // exactly the same eager fold equation as the reference verifier.
        cur = fold_statement_eager(
            cur, msg1, beta, msg2, alpha, pp.levels[j], GammaPrime, LambdaPrime);

        Omega = std::move(OmegaPrime);
        Theta = std::move(ThetaPrime);
        r0 = r0Prime;
        r1 = r1Prime;
        r2 = r2Prime;
    }

    if (timing_profile != nullptr) {
        timing_profile->recursive_prover_ms +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - recursive_start).count();
    }

    if (cur.n != 1U || Omega.size() != 1U || Theta.size() != 1U) {
        throw std::logic_error("recursive Dory prover did not reach n=1");
    }

    // PLAN §10 terminal step: map the folded singleton statement to Pi_do_sp.
    const ScalarProductStatement scalar_stmt{
        cur.Gamma[0], cur.Lambda[0], cur.Q, cur.D0, cur.D1, cur.D2};

    // PLAN §10 terminal step: map the folded singleton witness to Pi_do_sp.
    const ScalarProductWitness scalar_wit{
        Omega[0], Theta[0], r0, r1, r2};

    // PLAN §10 and §12: run the already-audited scalar-product prover at n=1.
    transcript.scalar_transcript = prove_scalar_product(
        scalar_stmt, scalar_wit, channel, pairing_profile, timing_profile);

    if (timing_profile != nullptr) {
        timing_profile->prove_ms +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - prove_start).count();
    }

    return transcript;
}

} // namespace dory
