#pragma once

#include "dory/dory_precomp.hpp"
#include "dory/gt_lincomb.hpp"

namespace dory {

// PLAN §13 and Milestone 3: eager reference verifier only. The deferred
// multi-exponentiation verifier is intentionally absent until Milestone 4.
bool verify_dory_eager_reference(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript);

// PLAN §14.2-§14.3: symbolic forms of the current folded D0, D1, and D2.
// Exposed so equivalence tests can compare their evaluations with eager folding.
struct DeferredDoryState {
    GtLinComb A0;
    GtLinComb A1;
    GtLinComb A2;
};

// PLAN §14 and Milestone 6a: non-invasive verifier instrumentation.
// Counts describe the symbolic state and final check without changing equations.
struct DoryVerifyStats {
    std::size_t recursive_rounds = 0;
    std::size_t final_check_terms = 0;
    std::size_t A0_terms = 0;
    std::size_t A1_terms = 0;
    std::size_t A2_terms = 0;
};

bool build_dory_deferred_state(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript,
    DeferredDoryState& out);

// PLAN §14: optimized-verifier shape with a simple Milestone 4 evaluator.
bool verify_dory_deferred_multiexp(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript);

bool verify_dory_deferred_multiexp(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript,
    DoryVerifyStats* stats);

bool verify_dory_deferred_multiexp(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript,
    DoryVerifyStats* stats,
    GtMultiexpMethod method);

// PLAN §14: the default verifier now selects deferred multi-exponentiation;
// the eager verifier remains available as the reference oracle.
bool verify_dory(
    const DoryPrecomp& pp,
    const DoryStatement& stmt,
    const DoryTranscript& transcript);

} // namespace dory
