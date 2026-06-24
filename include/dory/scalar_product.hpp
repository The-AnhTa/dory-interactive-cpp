#pragma once

#include "dory/challenge_channel.hpp"
#include "dory/dory_types.hpp"
#include "dory/profiling.hpp"

namespace dory {

// PLAN §12: interactive scalar-product prover Pi_do_sp. No recursive Dory,
// batching, transcript hashing, or Fiat-Shamir behavior is included here.
ScalarProductTranscript prove_scalar_product(
    const ScalarProductStatement& stmt,
    const ScalarProductWitness& wit,
    ChallengeChannel& channel);

ScalarProductTranscript prove_scalar_product(
    const ScalarProductStatement& stmt,
    const ScalarProductWitness& wit,
    ChallengeChannel& channel,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile);

// PLAN §12.3: eager verifier for one scalar-product transcript.
bool verify_scalar_product(
    const ScalarProductStatement& stmt,
    const ScalarProductTranscript& transcript);

} // namespace dory
