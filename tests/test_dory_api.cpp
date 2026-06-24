#include "dory/dory_api.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_single_api() {
    dory::DoryRunMetrics setup_metrics;
    const dory::DoryPrecomp pp = dory::make_dory_crs_for_size(16U, &setup_metrics);
    auto instance = dory::random_valid_dory_instance(pp);
    dory::DeterministicChallengeChannel channel(81001U);
    const dory::DorySingleProof result = dory::prove_dory_single(
        pp, instance.first, instance.second, channel);
    dory::DoryRunMetrics verify_metrics;
    require(dory::verify_dory_single(
        pp, result.statement, result.proof, &verify_metrics),
        "single facade proof rejected");
    require(setup_metrics.crs_bytes > 0U, "single CRS size is zero");
    require(result.metrics.proof_bytes > 0U, "single proof size is zero");
    require(result.metrics.prove_ms >= 0.0, "single prove time is negative");
    require(verify_metrics.verify_ms >= 0.0, "single verify time is negative");
    require(verify_metrics.accepted, "single accepted metric is false");
}

void test_batch_api() {
    dory::DoryRunMetrics setup_metrics;
    const dory::DoryPrecomp pp = dory::make_dory_crs_for_size(16U, &setup_metrics);
    std::vector<dory::DoryStatement> statements;
    std::vector<dory::DoryWitness> witnesses;
    for (std::size_t i = 0; i < 4U; ++i) {
        auto instance = dory::random_valid_dory_instance(pp);
        statements.push_back(std::move(instance.first));
        witnesses.push_back(std::move(instance.second));
    }
    dory::DeterministicChallengeChannel channel(81002U);
    const dory::DoryBatchProof result = dory::prove_dory_batch_api(
        pp, statements, witnesses, channel);
    dory::DoryRunMetrics verify_metrics;
    require(dory::verify_dory_batch_api(
        pp, result.statements, result.proof, &verify_metrics),
        "batch facade proof rejected");
    require(setup_metrics.crs_bytes > 0U, "batch CRS size is zero");
    require(result.metrics.proof_bytes > 0U, "batch proof size is zero");
    require(result.metrics.prove_ms >= 0.0, "batch prove time is negative");
    require(verify_metrics.verify_ms >= 0.0, "batch verify time is negative");
    require(verify_metrics.accepted, "batch accepted metric is false");
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_single_api();
        test_batch_api();
        std::cout << "Dory API tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Dory API tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
