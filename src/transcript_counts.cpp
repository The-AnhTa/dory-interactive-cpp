#include "dory/transcript_counts.hpp"

namespace dory {

DoryTranscriptCounts count_dory_transcript(const DoryTranscript& transcript) {
    DoryTranscriptCounts counts;
    counts.recursive_rounds = transcript.round_msg1.size();

    // PLAN §10: every recursive round contributes D1L,D1R,D2L,D2R,W1,W2.
    counts.gt_elements = 4U * transcript.round_msg1.size()
                       + 2U * transcript.round_msg2.size();

    // PLAN §12.1: scalar P1, P2, S, and R contribute four GT elements.
    counts.gt_elements += 4U;

    // PLAN §12.2: scalar E1 contributes one G1 and E2 contributes one G2.
    counts.g1_elements = 1U;
    counts.g2_elements = 1U;

    // PLAN §10: each recursive level records beta and alpha challenges.
    counts.fr_elements = transcript.beta_challenges.size()
                       + transcript.alpha_challenges.size();

    // PLAN §12: scalar transcript records epsilon, three responses, and theta.
    counts.fr_elements += 5U;
    return counts;
}

BatchDoryTranscriptCounts count_batch_dory_transcript(
    const BatchDoryTranscript& transcript) {
    BatchDoryTranscriptCounts counts;
    counts.batch_folds = transcript.folds.size();
    counts.final_dory = count_dory_transcript(transcript.final_dory);

    // PLAN §15: each batch fold records one GT Xcross and one Fr gamma.
    counts.gt_elements = counts.batch_folds + counts.final_dory.gt_elements;
    counts.g1_elements = counts.final_dory.g1_elements;
    counts.g2_elements = counts.final_dory.g2_elements;
    counts.fr_elements = counts.batch_folds + counts.final_dory.fr_elements;
    return counts;
}

} // namespace dory

