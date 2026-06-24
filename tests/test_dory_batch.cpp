#include "dory/dory_batch.hpp"

#include "dory/dory_verifier.hpp"
#include "dory/vector_ops.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

using Instances = std::pair<
    std::vector<dory::DoryStatement>,
    std::vector<dory::DoryWitness>>;

Instances make_instances(const dory::DoryPrecomp& pp, std::size_t count) {
    Instances out;
    out.first.reserve(count);
    out.second.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto instance = dory::random_valid_dory_instance(pp);
        out.first.push_back(std::move(instance.first));
        out.second.push_back(std::move(instance.second));
    }
    return out;
}

dory::GT nonidentity_gt() {
    return dory::pair(dory::g1_generator(), dory::g2_generator());
}

void require_verifier_rejected(
    const dory::DoryPrecomp& pp,
    const std::vector<dory::DoryStatement>& statements,
    const dory::BatchDoryTranscript& transcript,
    const char* message) {
    require(!dory::verify_batch_dory(pp, statements, transcript), message);
}

template <typename Action>
void require_invalid_argument(Action action, const char* message) {
    bool threw = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, message);
}

void test_honest_batches() {
    constexpr std::array<std::size_t, 5> batch_sizes{{1U, 2U, 3U, 4U, 8U}};
    constexpr std::array<std::size_t, 4> vector_sizes{{2U, 4U, 8U, 16U}};

    for (const std::size_t n : vector_sizes) {
        const dory::DoryPrecomp pp = dory::setup_dory_for_tests(n);
        for (const std::size_t ell : batch_sizes) {
            for (std::uint64_t trial = 0; trial < 3U; ++trial) {
                const Instances instances = make_instances(pp, ell);
                dory::DeterministicChallengeChannel channel(
                    400000U + static_cast<std::uint64_t>(n) * 1000U
                    + static_cast<std::uint64_t>(ell) * 10U + trial);
                const dory::BatchDoryTranscript transcript = dory::prove_batch_dory(
                    pp, instances.first, instances.second, channel);
                require(transcript.folds.size() == ell - 1U,
                        "batch transcript fold count differs");
                require(dory::verify_batch_dory(pp, instances.first, transcript),
                        "honest batch proof rejected");
            }
        }
    }
}

void test_direct_two_to_one_relation() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(8U);
    const Instances instances = make_instances(pp, 2U);
    const dory::DoryStatement& lhs_stmt = instances.first[0];
    const dory::DoryStatement& rhs_stmt = instances.first[1];
    const dory::DoryWitness& lhs_wit = instances.second[0];
    const dory::DoryWitness& rhs_wit = instances.second[1];
    const dory::Fr rX = dory::random_fr();
    const dory::Fr gamma = dory::random_nonzero_fr();

    // PLAN §15.1: Xcross = e(Omega,Theta') * e(Omega',Theta) * Q^rX.
    dory::GT Xcross = dory::gt_mul(
        dory::pair_product(lhs_wit.Omega, rhs_wit.Theta),
        dory::pair_product(rhs_wit.Omega, lhs_wit.Theta));
    Xcross = dory::gt_mul(Xcross, dory::gt_pow(pp.Q, rX));

    const dory::DoryStatement combined_stmt = dory::combine_batch_statements(
        lhs_stmt, rhs_stmt, Xcross, gamma);
    const dory::DoryWitness combined_wit = dory::combine_batch_witnesses(
        lhs_wit, rhs_wit, rX, gamma);

    // PLAN §15.1 direct relation: D0Star = e(OmegaStar,ThetaStar)*Q^r0Star.
    require(dory::gt_equal(
                combined_stmt.D0,
                dory::commit_pairing(
                    combined_wit.Omega, combined_wit.Theta,
                    combined_wit.r0, combined_stmt.Q)),
            "combined D0 relation failed");

    // PLAN §15.1 direct relation: D1Star = e(OmegaStar,Lambda)*Q^r1Star.
    require(dory::gt_equal(
                combined_stmt.D1,
                dory::commit_pairing(
                    combined_wit.Omega, combined_stmt.Lambda,
                    combined_wit.r1, combined_stmt.Q)),
            "combined D1 relation failed");

    // PLAN §15.1 direct relation: D2Star = e(Gamma,ThetaStar)*Q^r2Star.
    require(dory::gt_equal(
                combined_stmt.D2,
                dory::commit_pairing(
                    combined_stmt.Gamma, combined_wit.Theta,
                    combined_wit.r2, combined_stmt.Q)),
            "combined D2 relation failed");
}

void test_batch_tampering_rejected() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(8U);
    const Instances instances = make_instances(pp, 4U);
    dory::DeterministicChallengeChannel channel(515151U);
    const dory::BatchDoryTranscript honest = dory::prove_batch_dory(
        pp, instances.first, instances.second, channel);
    require(dory::verify_batch_dory(pp, instances.first, honest),
            "batch tamper baseline rejected");

    const dory::GT delta = nonidentity_gt();
    dory::BatchDoryTranscript tr = honest;

    tr.folds[0].Xcross = dory::gt_mul(tr.folds[0].Xcross, delta);
    require_verifier_rejected(pp, instances.first, tr, "tampered first Xcross accepted");

    tr = honest;
    tr.folds[0].gamma = tr.folds[0].gamma + dory::one_fr();
    require_verifier_rejected(pp, instances.first, tr, "tampered first gamma accepted");

    tr = honest;
    tr.folds.back().Xcross = dory::gt_mul(tr.folds.back().Xcross, delta);
    require_verifier_rejected(pp, instances.first, tr, "tampered last Xcross accepted");

    tr = honest;
    tr.folds.back().gamma = tr.folds.back().gamma + dory::one_fr();
    require_verifier_rejected(pp, instances.first, tr, "tampered last gamma accepted");

    tr = honest;
    tr.final_dory.round_msg1[0].D1L = dory::gt_mul(
        tr.final_dory.round_msg1[0].D1L, delta);
    require_verifier_rejected(pp, instances.first, tr, "tampered final D1L accepted");

    tr = honest;
    tr.final_dory.scalar_transcript.msg1.P1 = dory::gt_mul(
        tr.final_dory.scalar_transcript.msg1.P1, delta);
    require_verifier_rejected(pp, instances.first, tr, "tampered final P1 accepted");

    tr = honest;
    tr.final_dory.scalar_transcript.msg2.E1 = dory::g1_add(
        tr.final_dory.scalar_transcript.msg2.E1, dory::g1_generator());
    require_verifier_rejected(pp, instances.first, tr, "tampered final E1 accepted");

    std::vector<dory::DoryStatement> statements = instances.first;
    statements[1].D0 = dory::gt_mul(statements[1].D0, delta);
    require_verifier_rejected(pp, statements, honest, "tampered input D0 accepted");

    statements = instances.first;
    statements[1].D1 = dory::gt_mul(statements[1].D1, delta);
    require_verifier_rejected(pp, statements, honest, "tampered input D1 accepted");

    statements = instances.first;
    statements[1].D2 = dory::gt_mul(statements[1].D2, delta);
    require_verifier_rejected(pp, statements, honest, "tampered input D2 accepted");
}

void test_zero_gamma_rejected() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(4U);
    const Instances instances = make_instances(pp, 3U);
    dory::DeterministicChallengeChannel channel(616161U);
    dory::BatchDoryTranscript transcript = dory::prove_batch_dory(
        pp, instances.first, instances.second, channel);
    transcript.folds[0].gamma = dory::zero_fr();
    require_verifier_rejected(pp, instances.first, transcript, "zero gamma accepted");
}

void test_invalid_inputs_rejected() {
    const dory::DoryPrecomp pp = dory::setup_dory_for_tests(4U);
    const Instances instances = make_instances(pp, 2U);

    require_invalid_argument([&] {
        dory::DeterministicChallengeChannel channel(1U);
        static_cast<void>(dory::prove_batch_dory(pp, {}, {}, channel));
    }, "empty batch accepted");

    require_invalid_argument([&] {
        dory::DeterministicChallengeChannel channel(2U);
        static_cast<void>(dory::prove_batch_dory(
            pp, instances.first, {instances.second[0]}, channel));
    }, "statement/witness count mismatch accepted");

    std::vector<dory::DoryStatement> statements = instances.first;
    statements[1].Gamma[0] = dory::g1_add(
        statements[1].Gamma[0], dory::g1_generator());
    require_invalid_argument([&] {
        dory::DeterministicChallengeChannel channel(3U);
        static_cast<void>(dory::prove_batch_dory(
            pp, statements, instances.second, channel));
    }, "different Gamma accepted");

    statements = instances.first;
    statements[1].Lambda[0] = dory::g2_add(
        statements[1].Lambda[0], dory::g2_generator());
    require_invalid_argument([&] {
        dory::DeterministicChallengeChannel channel(4U);
        static_cast<void>(dory::prove_batch_dory(
            pp, statements, instances.second, channel));
    }, "different Lambda accepted");

    statements = instances.first;
    statements[1].Q = dory::gt_mul(statements[1].Q, nonidentity_gt());
    require_invalid_argument([&] {
        dory::DeterministicChallengeChannel channel(5U);
        static_cast<void>(dory::prove_batch_dory(
            pp, statements, instances.second, channel));
    }, "different Q accepted");

    statements = instances.first;
    statements[1].n = 2U;
    require_invalid_argument([&] {
        dory::DeterministicChallengeChannel channel(6U);
        static_cast<void>(dory::prove_batch_dory(
            pp, statements, instances.second, channel));
    }, "different n accepted");

    std::vector<dory::DoryWitness> witnesses = instances.second;
    witnesses[1].Omega.pop_back();
    require_invalid_argument([&] {
        dory::DeterministicChallengeChannel channel(7U);
        static_cast<void>(dory::prove_batch_dory(
            pp, instances.first, witnesses, channel));
    }, "inconsistent witness vector length accepted");

    dory::BatchDoryTranscript empty_transcript;
    require(!dory::verify_batch_dory(pp, {}, empty_transcript),
            "batch verifier accepted empty statement list");
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_honest_batches();
        test_direct_two_to_one_relation();
        test_batch_tampering_rejected();
        test_zero_gamma_rejected();
        test_invalid_inputs_rejected();
        std::cout << "Milestone 5 sequential batch tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Milestone 5 batch test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

