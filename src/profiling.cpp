#include "dory/profiling.hpp"

#include <stdexcept>

namespace dory {
namespace {

void record_pair_product(
    PairingProfile& profile,
    PairingProfileLabel label,
    std::size_t terms) {
    ++profile.pair_product_calls;
    profile.pair_product_total_terms += terms;
    switch (label) {
    case PairingProfileLabel::Setup:
        ++profile.setup_pair_product_calls;
        profile.setup_pair_product_terms += terms;
        break;
    case PairingProfileLabel::Instance:
        ++profile.instance_pair_product_calls;
        profile.instance_pair_product_terms += terms;
        break;
    case PairingProfileLabel::ProverD1:
        ++profile.prover_d1_pair_product_calls;
        profile.prover_d1_pair_product_terms += terms;
        break;
    case PairingProfileLabel::ProverD2:
        ++profile.prover_d2_pair_product_calls;
        profile.prover_d2_pair_product_terms += terms;
        break;
    case PairingProfileLabel::ProverW:
        ++profile.prover_w_pair_product_calls;
        profile.prover_w_pair_product_terms += terms;
        break;
    case PairingProfileLabel::BatchCross:
        ++profile.batch_cross_pair_product_calls;
        profile.batch_cross_pair_product_terms += terms;
        break;
    case PairingProfileLabel::Scalar:
    case PairingProfileLabel::Other:
        break;
    }
}

} // namespace

GT pair_profiled(
    const G1& a,
    const G2& b,
    PairingProfile* profile,
    PairingProfileLabel label) {
    if (profile != nullptr) {
        ++profile->pair_calls;
        if (label == PairingProfileLabel::Scalar) {
            ++profile->scalar_pair_calls;
        }
    }
    return pair(a, b);
}

GT pair_product_profiled(
    const std::vector<G1>& A,
    const std::vector<G2>& B,
    PairingProfile* profile,
    PairingProfileLabel label) {
    if (A.size() != B.size()) {
        throw std::invalid_argument("pair_product vector lengths differ");
    }
    if (profile != nullptr) {
        record_pair_product(*profile, label, A.size());
    }

    if (profile != nullptr) {
        // Logical pair-term usage is independent of whether mcl evaluates those
        // terms with individual final exponentiations or one multi-pairing.
        profile->pair_calls += A.size();
    }
    return pair_product(A, B);
}

} // namespace dory
