#pragma once

#include "dory/challenge_channel.hpp"
#include "dory/dory_precomp.hpp"

namespace dory {

// PLAN §11: shared eager statement fold used by prover and eager verifier.
DoryStatement fold_statement_eager(
    const DoryStatement& cur,
    const DoryRoundMsg1& msg1,
    const Fr& beta,
    const DoryRoundMsg2& msg2,
    const Fr& alpha,
    const DoryLevelPrecomp& pc,
    const std::vector<G1>& GammaPrime,
    const std::vector<G2>& LambdaPrime);

// PLAN §10: interactive recursive Dory prover Pi_do_ip.
DoryTranscript prove_dory_interactive(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryWitness& wit,
    ChallengeChannel& channel);

DoryTranscript prove_dory_interactive(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryWitness& wit,
    ChallengeChannel& channel,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile);

} // namespace dory
