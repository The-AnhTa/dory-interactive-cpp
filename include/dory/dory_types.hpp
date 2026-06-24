#pragma once

#include "dory/backend.hpp"

#include <cstddef>
#include <vector>

namespace dory {

// PLAN §12: public inputs for the scalar-product relation. The member types
// enforce Gamma in G1, Lambda in G2, and D0/D1/D2/Q in GT.
struct ScalarProductStatement {
    G1 Gamma;
    G2 Lambda;
    GT Q;
    GT D0;
    GT D1;
    GT D2;
};

// PLAN §12: witness for the scalar-product relation. The member types enforce
// Omega in G1, Theta in G2, and r0/r1/r2 in Fr.
struct ScalarProductWitness {
    G1 Omega;
    G2 Theta;
    Fr r0;
    Fr r1;
    Fr r2;
};

// PLAN §5 and §12.1: first scalar-product prover message, entirely in GT.
struct ScalarProductMsg1 {
    GT P1;
    GT P2;
    GT S;
    GT R;
};

// PLAN §5 and §12.2: second scalar-product prover message. E1 and E2 retain
// distinct source-group types, preventing same-group pairings by construction.
struct ScalarProductMsg2 {
    G1 E1;
    G2 E2;
    Fr r1_resp;
    Fr r2_resp;
    Fr r0_hat;
};

// PLAN §12: the interactive challenges are recorded for deterministic replay
// and for explicit verifier validation that epsilon and theta are nonzero.
struct ScalarProductTranscript {
    ScalarProductMsg1 msg1;
    Fr epsilon;
    ScalarProductMsg2 msg2;
    Fr theta;
};

// PLAN §3 and §5: public recursive Dory relation.
struct DoryStatement {
    std::size_t n;
    std::vector<G1> Gamma;
    std::vector<G2> Lambda;
    GT Q;
    GT D0;
    GT D1;
    GT D2;
};

// PLAN §3 and §5: recursive Dory witness.
struct DoryWitness {
    std::vector<G1> Omega;
    std::vector<G2> Theta;
    Fr r0;
    Fr r1;
    Fr r2;
};

// PLAN §5 and §10.1: first prover message at one recursive level.
struct DoryRoundMsg1 {
    GT D1L;
    GT D1R;
    GT D2L;
    GT D2R;
};

// PLAN §5 and §10.3: second prover message at one recursive level.
struct DoryRoundMsg2 {
    GT W1;
    GT W2;
};

// PLAN §5 and §10: recursive messages/challenges followed by Pi_do_sp at n=1.
struct DoryTranscript {
    std::vector<DoryRoundMsg1> round_msg1;
    std::vector<Fr> beta_challenges;
    std::vector<DoryRoundMsg2> round_msg2;
    std::vector<Fr> alpha_challenges;
    ScalarProductTranscript scalar_transcript;
};

} // namespace dory
