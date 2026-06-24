#include "dory/dory_verifier.hpp"

#include <utility>
#include <vector>

namespace dory {
namespace {

template <typename T>
bool vectors_equal(const std::vector<T>& a, const std::vector<T>& b) {
    return a == b;
}

bool validate_recursive_inputs(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript) {
    // PLAN §14.2: bind the statement to the same public precomputation used by
    // the prover and eager reference verifier.
    if (stmt.n != pp.n || stmt.n == 0U
        || pp.GammaLevels.size() != pp.log_n + 1U
        || pp.LambdaLevels.size() != pp.log_n + 1U
        || pp.levels.size() != pp.log_n
        || stmt.Gamma.size() != stmt.n || stmt.Lambda.size() != stmt.n
        || !vectors_equal(stmt.Gamma, pp.GammaLevels[0])
        || !vectors_equal(stmt.Lambda, pp.LambdaLevels[0])
        || !gt_equal(stmt.Q, pp.Q)) {
        return false;
    }

    // PLAN §14.3: every recursive transcript vector has exactly log_n entries.
    return transcript.round_msg1.size() == pp.log_n
        && transcript.beta_challenges.size() == pp.log_n
        && transcript.round_msg2.size() == pp.log_n
        && transcript.alpha_challenges.size() == pp.log_n;
}

} // namespace

bool build_dory_deferred_state(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript,
    DeferredDoryState& out) {
    if (!validate_recursive_inputs(pp, stmt, transcript)) {
        return false;
    }

    // PLAN §14.2: A0 = [(D0, 1)], representing current_D0 = eval(A0).
    GtLinComb A0;
    append_term(A0, stmt.D0, one_fr());

    // PLAN §14.2: A1 = [(D1, 1)], representing current_D1 = eval(A1).
    GtLinComb A1;
    append_term(A1, stmt.D1, one_fr());

    // PLAN §14.2: A2 = [(D2, 1)], representing current_D2 = eval(A2).
    GtLinComb A2;
    append_term(A2, stmt.D2, one_fr());

    for (std::size_t j = 0; j < pp.log_n; ++j) {
        const DoryRoundMsg1& msg1 = transcript.round_msg1[j];
        const DoryRoundMsg2& msg2 = transcript.round_msg2[j];
        const Fr& beta = transcript.beta_challenges[j];
        const Fr& alpha = transcript.alpha_challenges[j];

        // PLAN §14.3: alpha and beta are inverted, so reject zero challenges.
        if (is_zero(beta) || is_zero(alpha)) {
            return false;
        }

        // PLAN §14.3: beta_inv = beta^{-1}; alpha_inv = alpha^{-1}.
        const Fr beta_inv = inv(beta);
        const Fr alpha_inv = inv(alpha);
        const DoryLevelPrecomp& pc = pp.levels[j];

        // PLAN §14.3: deferred D0 folding.
        // Eager: D0' = D0*X*D1^beta_inv*D2^beta*W1^alpha*W2^alpha_inv.
        // Symbolic: append A0, beta_inv*A1, beta*A2, X, W1, and W2.
        GtLinComb newA0;
        append_scaled_terms(newA0, A0, one_fr());
        append_scaled_terms(newA0, A1, beta_inv);
        append_scaled_terms(newA0, A2, beta);
        append_term(newA0, pc.X, one_fr());
        append_term(newA0, msg2.W1, alpha);
        append_term(newA0, msg2.W2, alpha_inv);

        // PLAN §14.3: deferred D1 folding.
        // D1' = D1L^alpha*D1R*Delta1L^(alpha*beta)*Delta1R^beta.
        GtLinComb newA1;
        append_term(newA1, msg1.D1L, alpha);
        append_term(newA1, msg1.D1R, one_fr());
        append_term(newA1, pc.Delta1L, alpha * beta);
        append_term(newA1, pc.Delta1R, beta);

        // PLAN §14.3: deferred D2 folding.
        // D2' = D2L^alpha_inv*D2R*Delta2L^(alpha_inv*beta_inv)
        //       * Delta2R^beta_inv.
        GtLinComb newA2;
        append_term(newA2, msg1.D2L, alpha_inv);
        append_term(newA2, msg1.D2R, one_fr());
        append_term(newA2, pc.Delta2L, alpha_inv * beta_inv);
        append_term(newA2, pc.Delta2R, beta_inv);

        A0 = std::move(newA0);
        A1 = std::move(newA1);
        A2 = std::move(newA2);
    }

    out.A0 = std::move(A0);
    out.A1 = std::move(A1);
    out.A2 = std::move(A2);
    return true;
}

bool verify_dory_deferred_multiexp(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript) {
    return verify_dory_deferred_multiexp(pp, stmt, transcript, nullptr);
}

bool verify_dory_deferred_multiexp(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript,
    DoryVerifyStats* stats) {
    // Milestone 6b: raw final terms are 10 + 11 per recursive round.
    const std::size_t expected_terms =
        10U + 11U * transcript.round_msg1.size();
    return verify_dory_deferred_multiexp(
        pp, stmt, transcript, stats, choose_gt_multiexp_method(expected_terms));
}

bool verify_dory_deferred_multiexp(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript,
    DoryVerifyStats* stats,
    GtMultiexpMethod method) {
    if (stats != nullptr) {
        // Milestone 6a instrumentation: reset output before any validation path.
        *stats = DoryVerifyStats{};
    }

    DeferredDoryState state;
    if (!build_dory_deferred_state(pp, stmt, transcript, state)) {
        return false;
    }
    if (pp.GammaLevels[pp.log_n].size() != 1U
        || pp.LambdaLevels[pp.log_n].size() != 1U) {
        return false;
    }

    if (stats != nullptr) {
        // Milestone 6a instrumentation: record the fully folded symbolic state.
        stats->recursive_rounds = pp.log_n;
        stats->A0_terms = state.A0.size();
        stats->A1_terms = state.A1.size();
        stats->A2_terms = state.A2.size();
    }

    const ScalarProductTranscript& scalar = transcript.scalar_transcript;
    const ScalarProductMsg1& sm1 = scalar.msg1;
    const ScalarProductMsg2& sm2 = scalar.msg2;
    const Fr& epsilon = scalar.epsilon;
    const Fr& theta = scalar.theta;

    // PLAN §14.4: epsilon and theta must be nonzero; theta is inverted below.
    if (is_zero(epsilon) || is_zero(theta)) {
        return false;
    }

    // PLAN §14.4: Gamma_fin and Lambda_fin are final singleton generators.
    const G1& Gamma_fin = pp.GammaLevels[pp.log_n][0];
    const G2& Lambda_fin = pp.LambdaLevels[pp.log_n][0];

    // PLAN §14.4: theta_inv = theta^{-1}.
    const Fr theta_inv = inv(theta);

    // PLAN §14.4: lhs_g1 = E1 + theta * Gamma_fin.
    const G1 lhs_g1 = g1_add(sm2.E1, g1_mul(Gamma_fin, theta));

    // PLAN §14.4: lhs_g2 = E2 + theta_inv * Lambda_fin.
    const G2 lhs_g2 = g2_add(sm2.E2, g2_mul(Lambda_fin, theta_inv));

    // PLAN §14.4: lhs = e(lhs_g1, lhs_g2), type-correct G1 x G2 -> GT.
    const GT lhs = pair(lhs_g1, lhs_g2);

    // PLAN §14.4: X_fin = e(Gamma_fin, Lambda_fin).
    const GT X_fin = pair(Gamma_fin, Lambda_fin);

    // PLAN §14.4: r = r0_hat + theta*r2_resp + theta_inv*r1_resp.
    const Fr r = sm2.r0_hat
               + theta * sm2.r2_resp
               + theta_inv * sm2.r1_resp;

    GtLinComb check_terms;

    // PLAN §14.4: move lhs to the RHS product as lhs^(-1).
    append_term(check_terms, lhs, neg(one_fr()));

    // PLAN §14.4: append X_fin * R * S^epsilon.
    append_term(check_terms, X_fin, one_fr());
    append_term(check_terms, sm1.R, one_fr());
    append_term(check_terms, sm1.S, epsilon);

    // PLAN §14.4: append final_D0^(epsilon^2) as epsilon^2 * A0.
    append_scaled_terms(check_terms, state.A0, epsilon * epsilon);

    // PLAN §14.4: append P2^theta and final_D2^(theta*epsilon).
    append_term(check_terms, sm1.P2, theta);
    append_scaled_terms(check_terms, state.A2, theta * epsilon);

    // PLAN §14.4: append P1^theta_inv and final_D1^(theta_inv*epsilon).
    append_term(check_terms, sm1.P1, theta_inv);
    append_scaled_terms(check_terms, state.A1, theta_inv * epsilon);

    // PLAN §14.4: append Q^(-r), not Q^(+r), to remove commitment blinding.
    append_term(check_terms, pp.Q, neg(r));

    if (stats != nullptr) {
        // Milestone 6a instrumentation: count terms immediately before evaluation.
        stats->final_check_terms = check_terms.size();
    }

    // PLAN §14.4: accept iff the single symbolic product evaluates to 1_GT.
    return gt_equal(eval_gt_multiexp(check_terms, method), gt_one());
}

bool verify_dory(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript) {
    return verify_dory_deferred_multiexp(pp, stmt, transcript);
}

} // namespace dory
