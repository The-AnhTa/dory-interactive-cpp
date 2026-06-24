#pragma once

#include "dory/challenge_channel.hpp"
#include "dory/dory_precomp.hpp"

#include <vector>

namespace dory {

// PLAN §15: each batch fold reduces two Dory claims to one claim.
// Xcross commits to e(Omega,Theta') * e(Omega',Theta), including Q blinding.
struct BatchDoryFoldMsg {
    GT Xcross;
    Fr gamma;
};

// PLAN §15.2: sequential fold messages followed by one ordinary Dory proof.
struct BatchDoryTranscript {
    std::vector<BatchDoryFoldMsg> folds;
    DoryTranscript final_dory;
};

// PLAN §15.1: public two-to-one claim combination.
DoryStatement combine_batch_statements(
    const DoryStatement& lhs,
    const DoryStatement& rhs,
    const GT& Xcross,
    const Fr& gamma);

// PLAN §15.1: private two-to-one witness combination.
DoryWitness combine_batch_witnesses(
    const DoryWitness& lhs,
    const DoryWitness& rhs,
    const Fr& rX,
    const Fr& gamma);

BatchDoryTranscript prove_batch_dory(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements,
    const std::vector<DoryWitness>& witnesses,
    ChallengeChannel& channel);

BatchDoryTranscript prove_batch_dory(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements,
    const std::vector<DoryWitness>& witnesses,
    ChallengeChannel& channel,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile);

bool verify_batch_dory(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements,
    const BatchDoryTranscript& transcript);

} // namespace dory
