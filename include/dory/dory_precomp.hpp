#pragma once

#include "dory/dory_types.hpp"
#include "dory/profiling.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace dory {

// PLAN §7: target-group values fixed by public generators at one level.
struct DoryLevelPrecomp {
    GT X;
    GT Delta1L;
    GT Delta1R;
    GT Delta2L;
    GT Delta2R;
};

// PLAN §7: public generator levels and their pairing precomputations.
struct DoryPrecomp {
    std::size_t n;
    std::size_t log_n;
    std::vector<std::vector<G1>> GammaLevels;
    std::vector<std::vector<G2>> LambdaLevels;
    std::vector<DoryLevelPrecomp> levels;
    GT Q;
};

enum class DorySetupMethod {
    SimplePrecomp,
    OptimizedPrecomp
};

DoryPrecomp setup_dory_for_tests(std::size_t n);
DoryPrecomp setup_dory_for_tests(
    std::size_t n,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile);

DoryPrecomp setup_dory_for_tests(
    std::size_t n,
    DorySetupMethod method,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile);

DoryPrecomp setup_dory_from_generators_simple(
    const std::vector<G1>& Gamma,
    const std::vector<G2>& Lambda,
    const GT& Q,
    PairingProfile* pairing_profile = nullptr,
    TimingProfile* timing_profile = nullptr);

DoryPrecomp setup_dory_from_generators_optimized(
    const std::vector<G1>& Gamma,
    const std::vector<G2>& Lambda,
    const GT& Q,
    PairingProfile* pairing_profile = nullptr,
    TimingProfile* timing_profile = nullptr);

DoryPrecomp setup_dory_from_generators(
    const std::vector<G1>& Gamma,
    const std::vector<G2>& Lambda,
    const GT& Q);

DoryPrecomp setup_dory_from_generators(
    const std::vector<G1>& Gamma,
    const std::vector<G2>& Lambda,
    const GT& Q,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile);

// PLAN §8: construct an honest random witness and its three public commitments.
std::pair<DoryStatement, DoryWitness>
random_valid_dory_instance(const DoryPrecomp& pp);

std::pair<DoryStatement, DoryWitness> random_valid_dory_instance(
    const DoryPrecomp& pp,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile);

} // namespace dory
