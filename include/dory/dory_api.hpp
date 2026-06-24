#pragma once

#include "dory/dory_batch.hpp"

#include <cstddef>
#include <vector>

namespace dory {

// Cleanup API facade: operational metrics around the existing protocol calls.
struct DoryRunMetrics {
    double precompute_ms = 0.0;
    std::size_t crs_bytes = 0;
    double prove_ms = 0.0;
    std::size_t proof_bytes = 0;
    double verify_ms = 0.0;
    bool accepted = false;
};

struct DorySingleProof {
    DoryStatement statement;
    DoryTranscript proof;
    DoryRunMetrics metrics;
};

struct DoryBatchProof {
    std::vector<DoryStatement> statements;
    BatchDoryTranscript proof;
    DoryRunMetrics metrics;
};

// Wraps the existing optimized public test setup.
DoryPrecomp make_dory_crs_for_size(
    std::size_t n,
    DoryRunMetrics* metrics = nullptr);

// Encoded sizes use the existing mcl-backed serialization helpers, not sizeof.
std::size_t dory_crs_size_bytes(const DoryPrecomp& pp);
std::size_t dory_proof_size_bytes(const DoryTranscript& proof);
std::size_t dory_batch_proof_size_bytes(const BatchDoryTranscript& proof);

DorySingleProof prove_dory_single(
    const DoryPrecomp& pp,
    const DoryStatement& statement,
    const DoryWitness& witness,
    ChallengeChannel& channel);

bool verify_dory_single(
    const DoryPrecomp& pp,
    const DoryStatement& statement,
    const DoryTranscript& proof,
    DoryRunMetrics* metrics = nullptr);

DoryBatchProof prove_dory_batch_api(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements,
    const std::vector<DoryWitness>& witnesses,
    ChallengeChannel& channel);

bool verify_dory_batch_api(
    const DoryPrecomp& pp,
    const std::vector<DoryStatement>& statements,
    const BatchDoryTranscript& proof,
    DoryRunMetrics* metrics = nullptr);

} // namespace dory
