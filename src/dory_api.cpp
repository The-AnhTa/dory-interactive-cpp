#include "dory/dory_api.hpp"

#include "dory/dory_prover.hpp"
#include "dory/dory_verifier.hpp"
#include "dory/serialization.hpp"

#include <chrono>

namespace dory {
namespace {

using Clock = std::chrono::steady_clock;

double elapsed_ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

} // namespace

DoryPrecomp make_dory_crs_for_size(
    std::size_t n,
    DoryRunMetrics* metrics) {
    const Clock::time_point start = Clock::now();
    // Cleanup facade: setup_dory_for_tests dispatches to OptimizedPrecomp.
    DoryPrecomp pp = setup_dory_for_tests(n);
    if (metrics != nullptr) {
        metrics->precompute_ms = elapsed_ms(start);
        metrics->crs_bytes = dory_crs_size_bytes(pp);
    }
    return pp;
}

std::size_t dory_crs_size_bytes(const DoryPrecomp& pp) {
    // Encoded CRS definition: Q, all generator levels, then all five GT values
    // at every recursive level. Each helper delegates encoding to mcl.
    std::size_t bytes = serialize_gt(pp.Q).size();
    for (const std::vector<G1>& level : pp.GammaLevels) {
        for (const G1& value : level) bytes += serialize_g1(value).size();
    }
    for (const std::vector<G2>& level : pp.LambdaLevels) {
        for (const G2& value : level) bytes += serialize_g2(value).size();
    }
    for (const DoryLevelPrecomp& level : pp.levels) {
        bytes += serialize_gt(level.X).size();
        bytes += serialize_gt(level.Delta1L).size();
        bytes += serialize_gt(level.Delta1R).size();
        bytes += serialize_gt(level.Delta2L).size();
        bytes += serialize_gt(level.Delta2R).size();
    }
    return bytes;
}

std::size_t dory_proof_size_bytes(const DoryTranscript& proof) {
    return serialized_size_bytes(proof);
}

std::size_t dory_batch_proof_size_bytes(const BatchDoryTranscript& proof) {
    return serialized_size_bytes(proof);
}

DorySingleProof prove_dory_single(
    const DoryPrecomp& pp,
    const DoryStatement& statement,
    const DoryWitness& witness,
    ChallengeChannel& channel) {
    DorySingleProof result;
    result.statement = statement;
    const Clock::time_point start = Clock::now();
    // Cleanup facade: delegate unchanged to the existing interactive prover.
    result.proof = prove_dory_interactive(pp, statement, witness, channel);
    result.metrics.prove_ms = elapsed_ms(start);
    result.metrics.proof_bytes = dory_proof_size_bytes(result.proof);
    return result;
}

bool verify_dory_single(
    const DoryPrecomp& pp,
    const DoryStatement& statement,
    const DoryTranscript& proof,
    DoryRunMetrics* metrics) {
    const Clock::time_point start = Clock::now();
    // Cleanup facade: delegate unchanged to the default optimized verifier.
    const bool accepted = verify_dory(pp, statement, proof);
    if (metrics != nullptr) {
        metrics->verify_ms = elapsed_ms(start);
        metrics->accepted = accepted;
        metrics->proof_bytes = dory_proof_size_bytes(proof);
    }
    return accepted;
}

DoryBatchProof prove_dory_batch_api(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements,
    const std::vector<DoryWitness>& witnesses,
    ChallengeChannel& channel) {
    DoryBatchProof result;
    result.statements = statements;
    const Clock::time_point start = Clock::now();
    // Cleanup facade: delegate unchanged to sequential batching.
    result.proof = prove_batch_dory(pp, statements, witnesses, channel);
    result.metrics.prove_ms = elapsed_ms(start);
    result.metrics.proof_bytes = dory_batch_proof_size_bytes(result.proof);
    return result;
}

bool verify_dory_batch_api(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements,
    const BatchDoryTranscript& proof,
    DoryRunMetrics* metrics) {
    const Clock::time_point start = Clock::now();
    // Cleanup facade: delegate unchanged to the existing batch verifier.
    const bool accepted = verify_batch_dory(pp, statements, proof);
    if (metrics != nullptr) {
        metrics->verify_ms = elapsed_ms(start);
        metrics->accepted = accepted;
        metrics->proof_bytes = dory_batch_proof_size_bytes(proof);
    }
    return accepted;
}

} // namespace dory
