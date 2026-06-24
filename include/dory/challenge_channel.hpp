#pragma once

#include "dory/backend.hpp"

#include <cstddef>
#include <cstdint>

namespace dory {

// PLAN §9: interactive challenge boundary. Milestone 2 consumes only epsilon
// and theta; the remaining methods preserve the planned interface without
// implementing recursive Dory or batching.
class ChallengeChannel {
public:
    virtual ~ChallengeChannel() = default;

    virtual Fr get_beta(std::size_t level) = 0;
    virtual Fr get_alpha(std::size_t level) = 0;
    virtual Fr get_scalar_epsilon() = 0;
    virtual Fr get_scalar_theta() = 0;
    virtual Fr get_batch_gamma(std::size_t index) = 0;
};

class RandomChallengeChannel final : public ChallengeChannel {
public:
    Fr get_beta(std::size_t level) override;
    Fr get_alpha(std::size_t level) override;
    Fr get_scalar_epsilon() override;
    Fr get_scalar_theta() override;
    Fr get_batch_gamma(std::size_t index) override;
};

// PLAN §9: deterministic nonzero challenges make interactive tests repeatable.
// This counter-based test channel is not a Fiat-Shamir transform.
class DeterministicChallengeChannel final : public ChallengeChannel {
public:
    explicit DeterministicChallengeChannel(std::uint64_t seed = 1U);

    Fr get_beta(std::size_t level) override;
    Fr get_alpha(std::size_t level) override;
    Fr get_scalar_epsilon() override;
    Fr get_scalar_theta() override;
    Fr get_batch_gamma(std::size_t index) override;

private:
    Fr challenge(std::uint64_t domain, std::uint64_t index) const;
    std::uint64_t seed_;
};

} // namespace dory

