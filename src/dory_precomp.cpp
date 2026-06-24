#include "dory/dory_precomp.hpp"

#include "dory/vector_ops.hpp"

#include <chrono>
#include <stdexcept>
#include <string>

namespace dory {

namespace {
using Clock = std::chrono::steady_clock;
double elapsed_ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
} // namespace

DoryPrecomp setup_dory_from_generators(
    const std::vector<G1>& Gamma,
    const std::vector<G2>& Lambda,
    const GT& Q) {
    return setup_dory_from_generators_optimized(
        Gamma, Lambda, Q, nullptr, nullptr);
}

DoryPrecomp setup_dory_from_generators(
    const std::vector<G1>& Gamma,
    const std::vector<G2>& Lambda,
    const GT& Q,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile) {
    return setup_dory_from_generators_optimized(
        Gamma, Lambda, Q, pairing_profile, timing_profile);
}

DoryPrecomp setup_dory_from_generators_simple(
    const std::vector<G1>& Gamma,
    const std::vector<G2>& Lambda,
    const GT& Q,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile) {
    const auto precompute_start = Clock::now();
    if (Gamma.empty()) {
        throw std::invalid_argument("Dory setup requires n > 0");
    }
    if (Gamma.size() != Lambda.size()) {
        throw std::invalid_argument("Gamma and Lambda sizes differ");
    }
    if (!is_power_of_two(Gamma.size())) {
        throw std::invalid_argument("Dory setup size must be a power of two");
    }

    DoryPrecomp pp;
    pp.n = Gamma.size();
    pp.log_n = log2_exact(pp.n);
    pp.Q = Q;
    pp.GammaLevels.push_back(Gamma);
    pp.LambdaLevels.push_back(Lambda);
    pp.levels.reserve(pp.log_n);

    for (std::size_t j = 0; j < pp.log_n; ++j) {
        const auto gamma_halves = split_half(pp.GammaLevels[j]);
        const auto lambda_halves = split_half(pp.LambdaLevels[j]);

        // PLAN §7 equation: Gamma_{j+1} = left_half(Gamma_j).
        pp.GammaLevels.push_back(gamma_halves.first);

        // PLAN §7 equation: Lambda_{j+1} = left_half(Lambda_j).
        pp.LambdaLevels.push_back(lambda_halves.first);

        DoryLevelPrecomp pc;

        // PLAN §7 equation: X_j = e(Gamma_j, Lambda_j)
        // = product_i e(Gamma_j[i], Lambda_j[i]).
        pc.X = pair_product_profiled(
            pp.GammaLevels[j], pp.LambdaLevels[j], pairing_profile,
            PairingProfileLabel::Setup);

        // PLAN §7 equation:
        // Delta1L_j = e(left_half(Gamma_j), Lambda_{j+1}).
        pc.Delta1L = pair_product_profiled(
            gamma_halves.first, pp.LambdaLevels[j + 1U], pairing_profile,
            PairingProfileLabel::Setup);

        // PLAN §7 equation:
        // Delta1R_j = e(right_half(Gamma_j), Lambda_{j+1}).
        pc.Delta1R = pair_product_profiled(
            gamma_halves.second, pp.LambdaLevels[j + 1U], pairing_profile,
            PairingProfileLabel::Setup);

        // PLAN §7 equation:
        // Delta2L_j = e(Gamma_{j+1}, left_half(Lambda_j)).
        pc.Delta2L = pair_product_profiled(
            pp.GammaLevels[j + 1U], lambda_halves.first, pairing_profile,
            PairingProfileLabel::Setup);

        // PLAN §7 equation:
        // Delta2R_j = e(Gamma_{j+1}, right_half(Lambda_j)).
        pc.Delta2R = pair_product_profiled(
            pp.GammaLevels[j + 1U], lambda_halves.second, pairing_profile,
            PairingProfileLabel::Setup);

        pp.levels.push_back(pc);
    }

    if (timing_profile != nullptr) {
        timing_profile->setup_precompute_ms += elapsed_ms(precompute_start);
    }
    return pp;
}

DoryPrecomp setup_dory_from_generators_optimized(
    const std::vector<G1>& Gamma,
    const std::vector<G2>& Lambda,
    const GT& Q,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile) {
    const auto precompute_start = Clock::now();
    if (Gamma.empty()) {
        throw std::invalid_argument("Dory setup requires n > 0");
    }
    if (Gamma.size() != Lambda.size()) {
        throw std::invalid_argument("Gamma and Lambda sizes differ");
    }
    if (!is_power_of_two(Gamma.size())) {
        throw std::invalid_argument("Dory setup size must be a power of two");
    }

    DoryPrecomp pp;
    pp.n = Gamma.size();
    pp.log_n = log2_exact(pp.n);
    pp.Q = Q;
    pp.GammaLevels.push_back(Gamma);
    pp.LambdaLevels.push_back(Lambda);

    // PLAN §7c: build all public generator levels before pairing computation.
    for (std::size_t j = 0; j < pp.log_n; ++j) {
        pp.GammaLevels.push_back(split_half(pp.GammaLevels[j]).first);
        pp.LambdaLevels.push_back(split_half(pp.LambdaLevels[j]).first);
    }
    pp.levels.resize(pp.log_n);

    if (pp.log_n > 0U) {
        std::vector<GT> X(pp.log_n + 1U);

        // PLAN §7c: X_log_n = e(Gamma_fin, Lambda_fin), one singleton term.
        X[pp.log_n] = pair_product_profiled(
            pp.GammaLevels[pp.log_n], pp.LambdaLevels[pp.log_n],
            pairing_profile, PairingProfileLabel::Setup);

        for (std::size_t j = pp.log_n; j-- > 0U;) {
            const auto gamma_halves = split_half(pp.GammaLevels[j]);
            const auto lambda_halves = split_half(pp.LambdaLevels[j]);

            // PLAN §7c: RR_j = e(GammaR, LambdaR).
            const GT RR = pair_product_profiled(
                gamma_halves.second, lambda_halves.second,
                pairing_profile, PairingProfileLabel::Setup);

            // PLAN §7c: Delta1R_j = e(GammaR, LambdaL).
            const GT Delta1R = pair_product_profiled(
                gamma_halves.second, lambda_halves.first,
                pairing_profile, PairingProfileLabel::Setup);

            // PLAN §7c: Delta2R_j = e(GammaL, LambdaR).
            const GT Delta2R = pair_product_profiled(
                gamma_halves.first, lambda_halves.second,
                pairing_profile, PairingProfileLabel::Setup);

            // PLAN §7c: X_j = X_{j+1} * RR_j.
            X[j] = gt_mul(X[j + 1U], RR);

            DoryLevelPrecomp& pc = pp.levels[j];
            pc.X = X[j];

            // PLAN §7c: Delta1L_j = e(GammaL,LambdaL) = X_{j+1}.
            pc.Delta1L = X[j + 1U];
            pc.Delta1R = Delta1R;

            // PLAN §7c: Delta2L_j = e(GammaL,LambdaL) = X_{j+1}.
            pc.Delta2L = X[j + 1U];
            pc.Delta2R = Delta2R;
        }
    }

    if (timing_profile != nullptr) {
        timing_profile->setup_precompute_ms += elapsed_ms(precompute_start);
    }
    return pp;
}

DoryPrecomp setup_dory_for_tests(std::size_t n) {
    return setup_dory_for_tests(n, nullptr, nullptr);
}

DoryPrecomp setup_dory_for_tests(
    std::size_t n,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile) {
    return setup_dory_for_tests(
        n, DorySetupMethod::OptimizedPrecomp,
        pairing_profile, timing_profile);
}

DoryPrecomp setup_dory_for_tests(
    std::size_t n,
    DorySetupMethod method,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile) {
    const auto setup_start = Clock::now();
    if (n == 0U) {
        throw std::invalid_argument("Dory test setup requires n > 0");
    }
    if (!is_power_of_two(n)) {
        throw std::invalid_argument("Dory test setup size must be a power of two");
    }

    std::vector<G1> Gamma;
    std::vector<G2> Lambda;
    Gamma.reserve(n);
    Lambda.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        // PLAN §7 test setup: Gamma_i = HashToG1("DORY_TEST_GAMMA_G1_i").
        Gamma.push_back(hash_to_g1_for_tests(
            "DORY_TEST_GAMMA_G1_" + std::to_string(i)));

        // PLAN §7 test setup: Lambda_i = HashToG2("DORY_TEST_LAMBDA_G2_i").
        Lambda.push_back(hash_to_g2_for_tests(
            "DORY_TEST_LAMBDA_G2_" + std::to_string(i)));
    }

    // PLAN §7 test setup: H1 = HashToG1("DORY_TEST_Q_H1").
    const G1 H1 = hash_to_g1_for_tests("DORY_TEST_Q_H1");

    // PLAN §7 test setup: H2 = HashToG2("DORY_TEST_Q_H2").
    const G2 H2 = hash_to_g2_for_tests("DORY_TEST_Q_H2");

    // PLAN §7 test setup: Q = e(H1, H2), with H1 in G1 and H2 in G2.
    const GT Q = pair_profiled(
        H1, H2, pairing_profile, PairingProfileLabel::Setup);
    if (g1_equal(H1, g1_zero()) || g2_equal(H2, g2_zero()) || gt_equal(Q, gt_one())) {
        throw std::runtime_error("domain-separated Dory test setup produced identity");
    }

    DoryPrecomp pp = method == DorySetupMethod::SimplePrecomp
        ? setup_dory_from_generators_simple(
            Gamma, Lambda, Q, pairing_profile, timing_profile)
        : setup_dory_from_generators_optimized(
            Gamma, Lambda, Q, pairing_profile, timing_profile);
    if (timing_profile != nullptr) {
        timing_profile->setup_ms += elapsed_ms(setup_start);
    }
    return pp;
}

std::pair<DoryStatement, DoryWitness>
random_valid_dory_instance(const DoryPrecomp& pp) {
    return random_valid_dory_instance(pp, nullptr, nullptr);
}

std::pair<DoryStatement, DoryWitness> random_valid_dory_instance(
    const DoryPrecomp& pp,
    PairingProfile* pairing_profile,
    TimingProfile* timing_profile) {
    const auto instance_start = Clock::now();
    if (pp.n == 0U || pp.GammaLevels.empty() || pp.LambdaLevels.empty()
        || pp.GammaLevels[0].size() != pp.n || pp.LambdaLevels[0].size() != pp.n) {
        throw std::invalid_argument("invalid Dory precomputation");
    }

    DoryWitness wit;
    wit.Omega.reserve(pp.n);
    wit.Theta.reserve(pp.n);
    for (std::size_t i = 0; i < pp.n; ++i) {
        // PLAN §8: Omega_i <- G1 and Theta_i <- G2 for the random valid witness.
        wit.Omega.push_back(random_g1_for_tests());
        wit.Theta.push_back(random_g2_for_tests());
    }
    // PLAN §8: r0, r1, r2 <- Fr.
    wit.r0 = random_fr();
    wit.r1 = random_fr();
    wit.r2 = random_fr();

    DoryStatement stmt;
    stmt.n = pp.n;
    stmt.Gamma = pp.GammaLevels[0];
    stmt.Lambda = pp.LambdaLevels[0];
    stmt.Q = pp.Q;

    // PLAN §8 equation: D0 = e(Omega, Theta) * Q^r0.
    const auto commit_start = Clock::now();
    stmt.D0 = gt_mul(
        pair_product_profiled(
            wit.Omega, wit.Theta, pairing_profile, PairingProfileLabel::Instance),
        gt_pow(stmt.Q, wit.r0));

    // PLAN §8 equation: D1 = e(Omega, Lambda) * Q^r1.
    stmt.D1 = gt_mul(
        pair_product_profiled(
            wit.Omega, stmt.Lambda, pairing_profile, PairingProfileLabel::Instance),
        gt_pow(stmt.Q, wit.r1));

    // PLAN §8 equation: D2 = e(Gamma, Theta) * Q^r2.
    stmt.D2 = gt_mul(
        pair_product_profiled(
            stmt.Gamma, wit.Theta, pairing_profile, PairingProfileLabel::Instance),
        gt_pow(stmt.Q, wit.r2));

    if (timing_profile != nullptr) {
        timing_profile->instance_commit_ms += elapsed_ms(commit_start);
        timing_profile->instance_ms += elapsed_ms(instance_start);
    }

    return {stmt, wit};
}

} // namespace dory
