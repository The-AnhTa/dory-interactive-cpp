#include "dory/dory_batch.hpp"
#include "dory/dory_prover.hpp"
#include "dory/dory_verifier.hpp"
#include "dory/serialization.hpp"
#include "dory/vector_ops.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Function>
void require_malformed(Function&& function, const char* message) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, message);
}

void test_elements() {
    const dory::Fr fr = dory::random_fr();
    const dory::G1 g1 = dory::random_g1_for_tests();
    const dory::G2 g2 = dory::random_g2_for_tests();
    const dory::GT gt = dory::pair(g1, g2);
    require(dory::deserialize_fr(dory::serialize_fr(fr)) == fr, "Fr roundtrip failed");
    require(dory::g1_equal(dory::deserialize_g1(dory::serialize_g1(g1)), g1), "G1 roundtrip failed");
    require(dory::g2_equal(dory::deserialize_g2(dory::serialize_g2(g2)), g2), "G2 roundtrip failed");
    require(dory::gt_equal(dory::deserialize_gt(dory::serialize_gt(gt)), gt), "GT roundtrip failed");
}

void test_statement_and_witness() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(8U);
    const auto instance = dory::random_valid_dory_instance(pp);
    const dory::DoryStatement statement = dory::deserialize_dory_statement(
        dory::serialize_dory_statement(instance.first));
    const dory::DoryWitness witness = dory::deserialize_dory_witness(
        dory::serialize_dory_witness(instance.second));
    require(statement.n == 8U && witness.Omega.size() == 8U, "instance lengths changed");
    require(dory::gt_equal(statement.D0, dory::commit_pairing(
        witness.Omega, witness.Theta, witness.r0, statement.Q)), "roundtrip D0 relation failed");
    require(dory::gt_equal(statement.D1, dory::commit_pairing(
        witness.Omega, statement.Lambda, witness.r1, statement.Q)), "roundtrip D1 relation failed");
    require(dory::gt_equal(statement.D2, dory::commit_pairing(
        statement.Gamma, witness.Theta, witness.r2, statement.Q)), "roundtrip D2 relation failed");
}

void test_dory_transcript() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(16U);
    const auto instance = dory::random_valid_dory_instance(pp);
    dory::DeterministicChallengeChannel channel(80001U);
    const dory::DoryTranscript proof = dory::prove_dory_interactive(
        pp, instance.first, instance.second, channel);
    require(dory::verify_dory(pp, instance.first, proof), "proof rejected before roundtrip");
    const std::string encoded = dory::serialize_dory_transcript(proof);
    require(dory::serialized_size_bytes(proof) == encoded.size(), "Dory size helper differs");
    const dory::DoryTranscript decoded = dory::deserialize_dory_transcript(encoded);
    require(dory::verify_dory(pp, instance.first, decoded), "proof rejected after roundtrip");

    const auto scalar_encoded = dory::serialize_scalar_product_transcript(proof.scalar_transcript);
    const auto scalar_decoded = dory::deserialize_scalar_product_transcript(scalar_encoded);
    dory::DoryTranscript scalar_replaced = proof;
    scalar_replaced.scalar_transcript = scalar_decoded;
    require(dory::verify_dory(pp, instance.first, scalar_replaced), "scalar transcript roundtrip failed");
}

void test_batch_transcript() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(16U);
    std::vector<dory::DoryStatement> statements;
    std::vector<dory::DoryWitness> witnesses;
    for (std::size_t i = 0; i < 4U; ++i) {
        auto instance = dory::random_valid_dory_instance(pp);
        statements.push_back(std::move(instance.first));
        witnesses.push_back(std::move(instance.second));
    }
    dory::DeterministicChallengeChannel channel(80002U);
    const dory::BatchDoryTranscript proof = dory::prove_batch_dory(
        pp, statements, witnesses, channel);
    const std::string encoded = dory::serialize_batch_dory_transcript(proof);
    require(dory::serialized_size_bytes(proof) == encoded.size(), "batch size helper differs");
    const auto decoded = dory::deserialize_batch_dory_transcript(encoded);
    const auto decoded_statements = dory::deserialize_dory_statements(
        dory::serialize_dory_statements(statements));
    require(dory::verify_batch_dory(pp, decoded_statements, decoded), "batch proof roundtrip failed");
}

void test_malformed_rejection() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(4U);
    const auto instance = dory::random_valid_dory_instance(pp);
    std::string statement = dory::serialize_dory_statement(instance.first);

    std::string wrong_version = statement;
    wrong_version.replace(0U, std::string("DORY_STATEMENT_V1").size(), "DORY_STATEMENT_V2");
    require_malformed([&] { dory::deserialize_dory_statement(wrong_version); }, "wrong version accepted");

    std::string wrong_curve = statement;
    const std::size_t curve = wrong_curve.find("BN254_MCL");
    wrong_curve.replace(curve, std::string("BN254_MCL").size(), "OTHER_CURVE");
    require_malformed([&] { dory::deserialize_dory_statement(wrong_curve); }, "wrong curve accepted");

    require_malformed([&] { dory::deserialize_dory_statement(statement.substr(0U, statement.size() / 2U)); },
                      "truncated statement accepted");

    std::string wrong_length = statement;
    const std::string old_count = "gamma_count 4";
    wrong_length.replace(wrong_length.find(old_count), old_count.size(), "gamma_count 3");
    require_malformed([&] { dory::deserialize_dory_statement(wrong_length); }, "wrong vector length accepted");

    std::string bad_scalar = dory::serialize_fr(dory::random_fr());
    const std::size_t value = bad_scalar.find("value ") + 6U;
    bad_scalar[value] = 'Z';
    require_malformed([&] { dory::deserialize_fr(bad_scalar); }, "invalid scalar accepted");
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_elements();
        test_statement_and_witness();
        test_dory_transcript();
        test_batch_transcript();
        test_malformed_rejection();
        std::cout << "serialization tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "serialization tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
