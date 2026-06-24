#include "dory/gt_lincomb.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

dory::GT random_gt() {
    return dory::pair(dory::random_g1_for_tests(), dory::random_g2_for_tests());
}

dory::Fr words_to_fr(const std::vector<std::uint64_t>& words) {
    std::vector<std::uint8_t> bytes(words.size() * 8U, 0U);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>(
            (words[i / 8U] >> (8U * (i % 8U))) & 0xffU);
    }
    dory::Fr out;
    out.setLittleEndianMod(bytes.data(), bytes.size());
    return out;
}

void test_scalar_bit_extraction() {
    const auto zero_words = dory::fr_to_words_le(dory::zero_fr());
    for (const std::uint64_t word : zero_words) {
        require(word == 0U, "zero scalar extracted a nonzero word");
    }
    for (std::size_t window = 0; window < 90U; ++window) {
        require(dory::get_window(dory::zero_fr(), window, 3U) == 0U,
                "zero scalar extracted a nonzero window");
    }

    require(dory::get_window(dory::one_fr(), 0U, 4U) == 1U,
            "one scalar window zero is not one");
    require(dory::get_window(dory::one_fr(), 1U, 4U) == 0U,
            "one scalar has unexpected upper bits");

    for (std::size_t trial = 0; trial < 100U; ++trial) {
        const dory::Fr scalar = dory::random_fr();
        const auto words = dory::fr_to_words_le(scalar);
        require(words_to_fr(words) == scalar,
                "canonical scalar words did not reconstruct Fr");
    }
}

void test_single_base_windowed_exponentiation() {
    for (std::size_t trial = 0; trial < 50U; ++trial) {
        const dory::GT base = random_gt();
        const dory::Fr scalar = dory::random_fr();
        const dory::GtLinComb positive{{base, scalar}};
        require(dory::gt_equal(
                    dory::gt_pow(base, scalar),
                    dory::eval_gt_multiexp_windowed_bucket(positive)),
                "single-base random windowed exponentiation differs");

        const dory::Fr negative = dory::neg(scalar);
        const dory::GtLinComb negative_terms{{base, negative}};
        require(dory::gt_equal(
                    dory::gt_pow(base, negative),
                    dory::eval_gt_multiexp_windowed_bucket(negative_terms)),
                "single-base negative windowed exponentiation differs");
    }
}

void test_normalization() {
    for (std::size_t trial = 0; trial < 30U; ++trial) {
        const dory::GT base = random_gt();
        dory::GtLinComb terms{
            {base, dory::random_fr()},
            {base, dory::random_fr()},
            {random_gt(), dory::zero_fr()},
            {dory::gt_one(), dory::random_fr()},
            {random_gt(), dory::neg(dory::random_fr())}};
        const dory::GtLinComb normalized = dory::normalize_gt_lincomb(terms);
        require(dory::gt_equal(
                    dory::eval_gt_multiexp_simple(terms),
                    dory::eval_gt_multiexp_simple(normalized)),
                "normalization changed GT expression value");
    }
}

void test_multiexp_equivalence() {
    constexpr std::array<std::size_t, 9> sizes{
        {0U, 1U, 2U, 4U, 8U, 16U, 32U, 64U, 128U}};

    for (const std::size_t size : sizes) {
        for (std::size_t trial = 0; trial < 10U; ++trial) {
            dory::GtLinComb terms;
            terms.reserve(size + 4U);
            const dory::GT duplicate = random_gt();
            for (std::size_t i = 0; i < size; ++i) {
                dory::Fr scalar = dory::random_fr();
                if (i % 11U == 0U) {
                    scalar = dory::zero_fr();
                } else if (i % 7U == 0U) {
                    scalar = dory::neg(scalar);
                }
                terms.push_back({i % 13U == 0U ? duplicate : random_gt(), scalar});
            }
            terms.push_back({dory::gt_one(), dory::random_fr()});

            require(dory::gt_equal(
                        dory::eval_gt_multiexp_simple(terms),
                        dory::eval_gt_multiexp_windowed_bucket(terms)),
                    "windowed bucket multi-exponentiation differs from simple");
        }
    }
}

void test_dispatch_policy() {
    require(dory::choose_gt_multiexp_method(47U)
                == dory::GtMultiexpMethod::Simple,
            "dispatch selected windowed below measured threshold");
    require(dory::choose_gt_multiexp_method(48U)
                == dory::GtMultiexpMethod::WindowedBucket,
            "dispatch did not select windowed at measured threshold");
}

} // namespace

int main() {
    try {
        dory::Backend::init_bn254();
        test_scalar_bit_extraction();
        test_single_base_windowed_exponentiation();
        test_normalization();
        test_multiexp_equivalence();
        test_dispatch_policy();
        std::cout << "Milestone 6b GT multi-exponentiation tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "GT multi-exponentiation test failure: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
