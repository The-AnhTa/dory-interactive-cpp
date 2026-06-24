#include "dory/dory_batch.hpp"

#include "dory/dory_prover.hpp"
#include "dory/dory_verifier.hpp"

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

bool same_public_parameters(
    const DoryStatement& lhs,
    const DoryStatement& rhs) {
    // PLAN §15 batching condition: all claims share n, Gamma, Lambda, and Q.
    return lhs.n == rhs.n
        && vectors_equal(lhs.Gamma, rhs.Gamma)
        && vectors_equal(lhs.Lambda, rhs.Lambda)
        && gt_equal(lhs.Q, rhs.Q);
}

bool statements_valid_for_precomp(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements) {
    if (statements.empty() || pp.GammaLevels.empty() || pp.LambdaLevels.empty()) {
        return false;
    }

    const DoryStatement& first = statements[0];
    if (first.n != pp.n || first.n == 0U
        || first.Gamma.size() != first.n || first.Lambda.size() != first.n
        || !vectors_equal(first.Gamma, pp.GammaLevels[0])
        || !vectors_equal(first.Lambda, pp.LambdaLevels[0])
        || !gt_equal(first.Q, pp.Q)) {
        return false;
    }

    for (const DoryStatement& stmt : statements) {
        if (stmt.Gamma.size() != stmt.n || stmt.Lambda.size() != stmt.n
            || !same_public_parameters(first, stmt)) {
            return false;
        }
    }
    return true;
}

} // namespace

DoryStatement combine_batch_statements(
    const DoryStatement& lhs,
    const DoryStatement& rhs,
    const GT& Xcross,
    const Fr& gamma) {
    if (is_zero(gamma)) {
        throw std::invalid_argument("batch gamma challenge must be nonzero");
    }
    if (!same_public_parameters(lhs, rhs)
        || lhs.Gamma.size() != lhs.n || lhs.Lambda.size() != lhs.n) {
        throw std::invalid_argument("batch statements have different parameters");
    }

    // PLAN §15.1: gamma_sq = gamma^2 in Fr.
    const Fr gamma_sq = gamma * gamma;

    DoryStatement out;
    // PLAN §15.1: n, Gamma, Lambda, and Q remain unchanged.
    out.n = lhs.n;
    out.Gamma = lhs.Gamma;
    out.Lambda = lhs.Lambda;
    out.Q = lhs.Q;

    // PLAN §15.1: D0Star = D0^(gamma^2) * Xcross^gamma * D0p.
    // This matches e(gamma*Omega+Omega', gamma*Theta+Theta') with blinding
    // gamma^2*r0 + gamma*rX + r0p.
    out.D0 = gt_mul(
        gt_mul(gt_pow(lhs.D0, gamma_sq), gt_pow(Xcross, gamma)),
        rhs.D0);

    // PLAN §15.1: D1Star = D1^gamma * D1p.
    out.D1 = gt_mul(gt_pow(lhs.D1, gamma), rhs.D1);

    // PLAN §15.1: D2Star = D2^gamma * D2p.
    out.D2 = gt_mul(gt_pow(lhs.D2, gamma), rhs.D2);
    return out;
}

DoryWitness combine_batch_witnesses(
    const DoryWitness& lhs,
    const DoryWitness& rhs,
    const Fr& rX,
    const Fr& gamma) {
    if (is_zero(gamma)) {
        throw std::invalid_argument("batch gamma challenge must be nonzero");
    }
    if (lhs.Omega.size() != lhs.Theta.size()
        || rhs.Omega.size() != rhs.Theta.size()
        || lhs.Omega.size() != rhs.Omega.size()) {
        throw std::invalid_argument("batch witness vector sizes differ");
    }

    DoryWitness out;
    out.Omega.reserve(lhs.Omega.size());
    out.Theta.reserve(lhs.Theta.size());
    for (std::size_t i = 0; i < lhs.Omega.size(); ++i) {
        // PLAN §15.1: OmegaStar[i] = gamma * Omega[i] + OmegaPrime[i].
        out.Omega.push_back(g1_add(g1_mul(lhs.Omega[i], gamma), rhs.Omega[i]));

        // PLAN §15.1: ThetaStar[i] = gamma * Theta[i] + ThetaPrime[i].
        out.Theta.push_back(g2_add(g2_mul(lhs.Theta[i], gamma), rhs.Theta[i]));
    }

    // PLAN §15.1: r0Star = gamma^2*r0 + gamma*rX + r0p.
    out.r0 = gamma * gamma * lhs.r0 + gamma * rX + rhs.r0;

    // PLAN §15.1: r1Star = gamma*r1 + r1p.
    out.r1 = gamma * lhs.r1 + rhs.r1;

    // PLAN §15.1: r2Star = gamma*r2 + r2p.
    out.r2 = gamma * lhs.r2 + rhs.r2;
    return out;
}

BatchDoryTranscript prove_batch_dory(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements,
    const std::vector<DoryWitness>& witnesses,
    ChallengeChannel& channel) {
    return prove_batch_dory(
        pp, statements, witnesses, channel, nullptr, nullptr);
}

BatchDoryTranscript prove_batch_dory(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements,
    const std::vector<DoryWitness>& witnesses,
    ChallengeChannel& channel,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile) {
    const auto batch_prove_start = std::chrono::steady_clock::now();
    if (!statements_valid_for_precomp(pp, statements)) {
        throw std::invalid_argument("invalid or incompatible batch statements");
    }
    if (witnesses.size() != statements.size()) {
        throw std::invalid_argument("batch statement/witness counts differ");
    }
    for (const DoryWitness& wit : witnesses) {
        if (wit.Omega.size() != pp.n || wit.Theta.size() != pp.n) {
            throw std::invalid_argument("batch witness vector length differs from n");
        }
    }

    DoryStatement acc_stmt = statements[0];
    DoryWitness acc_wit = witnesses[0];
    BatchDoryTranscript transcript;
    transcript.folds.reserve(statements.size() - 1U);

    const auto batch_fold_start = std::chrono::steady_clock::now();
    for (std::size_t i = 1U; i < statements.size(); ++i) {
        // PLAN §15.1: rX <- Fr for the cross-pairing commitment blinding.
        const Fr rX = random_fr();

        // PLAN §15.1: Xcross pairing part starts with e(acc_Omega, Theta_i).
        GT Xcross = pair_product_profiled(
            acc_wit.Omega, witnesses[i].Theta, pairing_profile,
            PairingProfileLabel::BatchCross);

        // PLAN §15.1: Xcross *= e(Omega_i, acc_Theta).
        Xcross = gt_mul(
            Xcross,
            pair_product_profiled(
                witnesses[i].Omega, acc_wit.Theta, pairing_profile,
                PairingProfileLabel::BatchCross));

        // PLAN §15.1: Xcross *= Q^rX.
        Xcross = gt_mul(Xcross, gt_pow(acc_stmt.Q, rX));

        // PLAN §15.1: gamma <- Fr* after Xcross is sent.
        const Fr gamma = channel.get_batch_gamma(i - 1U);
        if (is_zero(gamma)) {
            throw std::invalid_argument("batch gamma challenge must be nonzero");
        }
        transcript.folds.push_back(BatchDoryFoldMsg{Xcross, gamma});

        // PLAN §15.1: fold the accumulated and new public statements.
        acc_stmt = combine_batch_statements(
            acc_stmt, statements[i], Xcross, gamma);

        // PLAN §15.1: fold the accumulated and new private witnesses.
        acc_wit = combine_batch_witnesses(
            acc_wit, witnesses[i], rX, gamma);
    }

    if (timing_profile != nullptr) {
        timing_profile->batch_fold_ms +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - batch_fold_start).count();
    }

    // PLAN §15.2: prove one ordinary recursive Dory claim after all folds.
    // For ell=1 the loop is empty and this directly proves the sole claim.
    const auto final_dory_start = std::chrono::steady_clock::now();
    TimingProfile nested_timing;
    transcript.final_dory = prove_dory_interactive(
        pp, acc_stmt, acc_wit, channel, pairing_profile,
        timing_profile != nullptr ? &nested_timing : nullptr);
    if (timing_profile != nullptr) {
        timing_profile->final_dory_prove_ms +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - final_dory_start).count();
        timing_profile->recursive_prover_ms += nested_timing.recursive_prover_ms;
        timing_profile->scalar_prover_ms += nested_timing.scalar_prover_ms;
        timing_profile->prove_ms +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - batch_prove_start).count();
    }
    return transcript;
}

bool verify_batch_dory(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements,
    const BatchDoryTranscript& transcript) {
    if (!statements_valid_for_precomp(pp, statements)
        || transcript.folds.size() != statements.size() - 1U) {
        return false;
    }

    DoryStatement acc_stmt = statements[0];
    try {
        for (std::size_t i = 1U; i < statements.size(); ++i) {
            const BatchDoryFoldMsg& fold = transcript.folds[i - 1U];

            // PLAN §15.2: reject zero gamma before combining public statements.
            if (is_zero(fold.gamma)) {
                return false;
            }

            // PLAN §15.2: reconstruct the sequential accumulated public claim.
            acc_stmt = combine_batch_statements(
                acc_stmt, statements[i], fold.Xcross, fold.gamma);
        }
    } catch (...) {
        return false;
    }

    // PLAN §15.2: the final accumulated claim uses the default deferred verifier.
    // For ell=1 there are no folds and this is ordinary Dory verification.
    return verify_dory(pp, acc_stmt, transcript.final_dory);
}

} // namespace dory
