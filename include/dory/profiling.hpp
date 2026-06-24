#pragma once

#include "dory/backend.hpp"

#include <cstddef>
#include <vector>

namespace dory {

enum class PairingProfileLabel {
    Setup,
    Instance,
    ProverD1,
    ProverD2,
    ProverW,
    Scalar,
    BatchCross,
    Other
};

// Milestone 7a: non-invasive pairing call and vector-term counters.
struct PairingProfile {
    std::size_t pair_calls = 0;
    std::size_t pair_product_calls = 0;
    std::size_t pair_product_total_terms = 0;

    std::size_t setup_pair_product_calls = 0;
    std::size_t setup_pair_product_terms = 0;
    std::size_t instance_pair_product_calls = 0;
    std::size_t instance_pair_product_terms = 0;
    std::size_t prover_d1_pair_product_calls = 0;
    std::size_t prover_d1_pair_product_terms = 0;
    std::size_t prover_d2_pair_product_calls = 0;
    std::size_t prover_d2_pair_product_terms = 0;
    std::size_t prover_w_pair_product_calls = 0;
    std::size_t prover_w_pair_product_terms = 0;
    std::size_t scalar_pair_calls = 0;
    std::size_t batch_cross_pair_product_calls = 0;
    std::size_t batch_cross_pair_product_terms = 0;
};

// Milestone 7a: nested phase timings accumulated by optional profiled APIs.
struct TimingProfile {
    double setup_ms = 0.0;
    double instance_ms = 0.0;
    double prove_ms = 0.0;
    double verify_ms = 0.0;
    double setup_precompute_ms = 0.0;
    double instance_commit_ms = 0.0;
    double recursive_prover_ms = 0.0;
    double scalar_prover_ms = 0.0;
    double batch_fold_ms = 0.0;
    double final_dory_prove_ms = 0.0;
};

GT pair_profiled(
    const G1& a,
    const G2& b,
    PairingProfile* profile,
    PairingProfileLabel label);

GT pair_product_profiled(
    const std::vector<G1>& A,
    const std::vector<G2>& B,
    PairingProfile* profile,
    PairingProfileLabel label);

} // namespace dory

