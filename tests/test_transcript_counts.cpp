#include "dory/dory_prover.hpp"
#include "dory/transcript_counts.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_dory_counts() {
    constexpr std::array<std::size_t, 4> sizes{{1U, 2U, 4U, 8U}};
    for (const std::size_t n : sizes) {
        const dory::DoryPrecomp pp = dory::setup_dory_for_tests(n);
        const auto instance = dory::random_valid_dory_instance(pp);
        dory::DeterministicChallengeChannel channel(700000U + n);
        const dory::DoryTranscript transcript = dory::prove_dory_interactive(
            pp, instance.first, instance.second, channel);
        const dory::DoryTranscriptCounts counts =
            dory::count_dory_transcript(transcript);

        // Milestone 6a: r=log2(n), six recursive GTs, then four scalar GTs.
        require(counts.recursive_rounds == pp.log_n, "recursive round count differs");
        require(counts.gt_elements == 6U * pp.log_n + 4U, "GT count differs");
        require(counts.g1_elements == 1U, "G1 count differs");
        require(counts.g2_elements == 1U, "G2 count differs");

        // Milestone 6a: two recursive challenges per round plus five scalar Frs.
        require(counts.fr_elements == 2U * pp.log_n + 5U, "Fr count differs");
    }
}

void test_batch_counts() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(4U);
    std::vector<dory::DoryStatement> statements;
    std::vector<dory::DoryWitness> witnesses;
    for (std::size_t i = 0; i < 3U; ++i) {
        auto instance = dory::random_valid_dory_instance(pp);
        statements.push_back(std::move(instance.first));
        witnesses.push_back(std::move(instance.second));
    }
    dory::DeterministicChallengeChannel channel(710000U);
    const dory::BatchDoryTranscript transcript = dory::prove_batch_dory(
        pp, statements, witnesses, channel);
    const dory::BatchDoryTranscriptCounts counts =
        dory::count_batch_dory_transcript(transcript);

    require(counts.batch_folds == 2U, "batch fold count differs");
    require(counts.gt_elements == counts.final_dory.gt_elements + 2U,
            "total batch GT count differs");
    require(counts.fr_elements == counts.final_dory.fr_elements + 2U,
            "total batch Fr count differs");
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_dory_counts();
        test_batch_counts();
        std::cout << "Milestone 6a transcript count tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Transcript count test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

