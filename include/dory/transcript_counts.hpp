#pragma once

#include "dory/dory_batch.hpp"

#include <cstddef>

namespace dory {

// Milestone 6a: structural counts only; this is not byte serialization.
struct DoryTranscriptCounts {
    std::size_t gt_elements = 0;
    std::size_t g1_elements = 0;
    std::size_t g2_elements = 0;
    std::size_t fr_elements = 0;
    std::size_t recursive_rounds = 0;
};

DoryTranscriptCounts count_dory_transcript(const DoryTranscript& transcript);

// Milestone 6a: total batch counts include fold fields and final_dory fields.
struct BatchDoryTranscriptCounts {
    std::size_t gt_elements = 0;
    std::size_t g1_elements = 0;
    std::size_t g2_elements = 0;
    std::size_t fr_elements = 0;
    std::size_t batch_folds = 0;
    DoryTranscriptCounts final_dory;
};

BatchDoryTranscriptCounts count_batch_dory_transcript(
    const BatchDoryTranscript& transcript);

} // namespace dory

