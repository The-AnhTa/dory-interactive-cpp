#include "dory/challenge_channel.hpp"

#include <cstdint>

namespace dory {

Fr RandomChallengeChannel::get_beta(std::size_t) {
    // PLAN §9: beta is inverted later and must lie in Fr*.
    return random_nonzero_fr();
}

Fr RandomChallengeChannel::get_alpha(std::size_t) {
    // PLAN §9: alpha is inverted later and must lie in Fr*.
    return random_nonzero_fr();
}

Fr RandomChallengeChannel::get_scalar_epsilon() {
    // PLAN §9 and §12.2: epsilon is sampled nonzero for the reference protocol.
    return random_nonzero_fr();
}

Fr RandomChallengeChannel::get_scalar_theta() {
    // PLAN §9 and §12.3: theta is inverted and must lie in Fr*.
    return random_nonzero_fr();
}

Fr RandomChallengeChannel::get_batch_gamma(std::size_t) {
    // PLAN §9: gamma is reserved for a later batch milestone and is nonzero.
    return random_nonzero_fr();
}

DeterministicChallengeChannel::DeterministicChallengeChannel(std::uint64_t seed)
    : seed_(seed) {}

Fr DeterministicChallengeChannel::challenge(
    std::uint64_t domain,
    std::uint64_t index) const {
    // PLAN §9: simple deterministic test challenges. Keep the value in the
    // positive int64 range accepted by mcl and add one so it can never be zero.
    constexpr std::uint64_t modulus = 0x7ffffffffffffffeULL;
    const std::uint64_t mixed = seed_ + domain * 0x9e3779b97f4a7c15ULL + index;
    Fr out;
    out = static_cast<std::int64_t>((mixed % modulus) + 1U);
    return out;
}

Fr DeterministicChallengeChannel::get_beta(std::size_t level) {
    return challenge(1U, static_cast<std::uint64_t>(level));
}

Fr DeterministicChallengeChannel::get_alpha(std::size_t level) {
    return challenge(2U, static_cast<std::uint64_t>(level));
}

Fr DeterministicChallengeChannel::get_scalar_epsilon() {
    return challenge(3U, 0U);
}

Fr DeterministicChallengeChannel::get_scalar_theta() {
    return challenge(4U, 0U);
}

Fr DeterministicChallengeChannel::get_batch_gamma(std::size_t index) {
    return challenge(5U, static_cast<std::uint64_t>(index));
}

} // namespace dory

