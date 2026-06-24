#include "dory/dory_verifier.hpp"

#include "dory/dory_prover.hpp"
#include "dory/scalar_product.hpp"

#include <vector>

namespace dory {
namespace {

template <typename T>
bool vectors_equal(const std::vector<T>& a, const std::vector<T>& b) {
    return a == b;
}

} // namespace

bool verify_dory_eager_reference(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript) {
    // PLAN §13: bind the statement to the public precomputation.
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

    // PLAN §13: every recursive transcript vector has exactly log_n entries.
    if (transcript.round_msg1.size() != pp.log_n
        || transcript.beta_challenges.size() != pp.log_n
        || transcript.round_msg2.size() != pp.log_n
        || transcript.alpha_challenges.size() != pp.log_n) {
        return false;
    }

    DoryStatement cur = stmt;
    for (std::size_t j = 0; j < pp.log_n; ++j) {
        const Fr& beta = transcript.beta_challenges[j];
        const Fr& alpha = transcript.alpha_challenges[j];

        // PLAN §13: reject zero beta or alpha before eager folding/inversion.
        if (is_zero(beta) || is_zero(alpha)) {
            return false;
        }

        try {
            // PLAN §13 and §11: eagerly materialize D0', D1', and D2' using the
            // shared fold helper and the next singleton-bound generator level.
            cur = fold_statement_eager(
                cur,
                transcript.round_msg1[j],
                beta,
                transcript.round_msg2[j],
                alpha,
                pp.levels[j],
                pp.GammaLevels[j + 1U],
                pp.LambdaLevels[j + 1U]);
        } catch (...) {
            return false;
        }
    }

    if (cur.n != 1U || cur.Gamma.size() != 1U || cur.Lambda.size() != 1U) {
        return false;
    }

    // PLAN §13 terminal step: final folded Dory statement becomes Pi_do_sp input.
    const ScalarProductStatement scalar_stmt{
        cur.Gamma[0], cur.Lambda[0], cur.Q, cur.D0, cur.D1, cur.D2};

    // PLAN §13: n=1 has zero recursive rounds and reaches this check directly.
    return verify_scalar_product(scalar_stmt, transcript.scalar_transcript);
}

} // namespace dory

