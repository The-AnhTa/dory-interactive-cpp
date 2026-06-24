#include "dory/backend.hpp"

#include "dory/profiling.hpp"

#include <mutex>
#include <atomic>
#include <stdexcept>
#include <string_view>

namespace dory {
namespace {

std::atomic<PairProductMethod> pair_product_method{PairProductMethod::Auto};

void ensure_initialized() {
    static std::once_flag once;
    std::call_once(once, [] {
        // PLAN §1 and §4: select the BN254 pairing parameters before using any
        // Fr, G1, G2, or GT operation exposed by this wrapper.
        mcl::bn::initPairing(mcl::BN254);
    });
}

} // namespace

void Backend::init_bn254() {
    ensure_initialized();
}

Fr random_fr() {
    ensure_initialized();
    Fr out;
    out.setByCSPRNG();
    return out;
}

Fr random_nonzero_fr() {
    Fr out;
    do {
        out = random_fr();
    } while (out.isZero());
    return out;
}

bool is_zero(const Fr& x) { return x.isZero(); }

Fr zero_fr() {
    ensure_initialized();
    Fr out;
    out.clear();
    return out;
}

Fr one_fr() {
    ensure_initialized();
    Fr out;
    out = 1;
    return out;
}

Fr neg(const Fr& x) {
    Fr out;
    Fr::neg(out, x);
    return out;
}

Fr inv(const Fr& x) {
    if (x.isZero()) {
        throw std::invalid_argument("cannot invert zero in Fr");
    }
    Fr out;
    Fr::inv(out, x);
    return out;
}

G1 g1_zero() {
    ensure_initialized();
    G1 out;
    out.clear();
    return out;
}

G2 g2_zero() {
    ensure_initialized();
    G2 out;
    out.clear();
    return out;
}

G1 g1_generator() {
    ensure_initialized();
    // PLAN §4: mcl's getG1basePoint() is the identity for this BN254 setup.
    // Derive a stable, non-identity subgroup point with a domain-separated label.
    static const G1 generator = [] {
        constexpr std::string_view domain = "dory-bn254/g1-generator/v1";
        G1 out;
        mcl::bn::hashAndMapToG1(out, domain.data(), domain.size());
        return out;
    }();
    return generator;
}

G2 g2_generator() {
    ensure_initialized();
    // PLAN §4: derive a stable, non-identity subgroup point with a distinct
    // domain-separated G2 label.
    static const G2 generator = [] {
        constexpr std::string_view domain = "dory-bn254/g2-generator/v1";
        G2 out;
        mcl::bn::hashAndMapToG2(out, domain.data(), domain.size());
        return out;
    }();
    return generator;
}

G1 hash_to_g1_for_tests(std::string_view label) {
    ensure_initialized();
    G1 out;
    mcl::bn::hashAndMapToG1(out, label.data(), label.size());
    return out;
}

G2 hash_to_g2_for_tests(std::string_view label) {
    ensure_initialized();
    G2 out;
    mcl::bn::hashAndMapToG2(out, label.data(), label.size());
    return out;
}

G1 random_g1_for_tests() {
    // PLAN §4 security note: s*P is convenient test sampling, not transparent
    // production generator setup. Additive source-group notation is used.
    return g1_mul(g1_generator(), random_nonzero_fr());
}

G2 random_g2_for_tests() {
    // PLAN §4 security note: s*Q is convenient test sampling, not transparent
    // production generator setup. Additive source-group notation is used.
    return g2_mul(g2_generator(), random_nonzero_fr());
}

G1 g1_add(const G1& a, const G1& b) {
    // PLAN §2: paper source-group multiplication A*B translates to A+B.
    G1 out;
    G1::add(out, a, b);
    return out;
}

G2 g2_add(const G2& a, const G2& b) {
    // PLAN §2: paper source-group multiplication A*B translates to A+B.
    G2 out;
    G2::add(out, a, b);
    return out;
}

G1 g1_mul(const G1& a, const Fr& s) {
    // PLAN §2: paper source-group exponentiation A^s translates to s*A.
    G1 out;
    G1::mul(out, a, s);
    return out;
}

G2 g2_mul(const G2& a, const Fr& s) {
    // PLAN §2: paper source-group exponentiation A^s translates to s*A.
    G2 out;
    G2::mul(out, a, s);
    return out;
}

bool g1_equal(const G1& a, const G1& b) { return a == b; }
bool g2_equal(const G2& a, const G2& b) { return a == b; }

GT gt_one() {
    ensure_initialized();
    // PLAN §2: 1_GT is the multiplicative target-group identity.
    GT out;
    out.setOne();
    return out;
}

GT gt_mul(const GT& a, const GT& b) {
    // PLAN §2: target groups retain paper multiplication, X*Y.
    GT out;
    GT::mul(out, a, b);
    return out;
}

GT gt_pow(const GT& a, const Fr& s) {
    // PLAN §2 and §4: target-group paper exponentiation X^s remains X^s.
    GT out;
    GT::pow(out, a, s);
    return out;
}

GT gt_inv(const GT& a) {
    GT out;
    GT::inv(out, a);
    return out;
}

bool gt_equal(const GT& a, const GT& b) { return a == b; }

GT pair(const G1& a, const G2& b) {
    ensure_initialized();
    // PLAN §1 and §4: e : G1 x G2 -> GT is the only exposed pairing direction.
    GT out;
    mcl::bn::pairing(out, a, b);
    return out;
}

GT pair_product(const std::vector<G1>& A, const std::vector<G2>& B) {
    PairProductMethod method = get_pair_product_method();
    if (method == PairProductMethod::Auto) {
        method = choose_pair_product_method(A.size());
    }
    return pair_product(A, B, method);
}

GT pair_product_simple(const std::vector<G1>& A, const std::vector<G2>& B) {
    if (A.size() != B.size()) {
        throw std::invalid_argument("pair_product vector lengths differ");
    }
    // PLAN §4 reference: product_i e(A_i,B_i), including one final exponentiation
    // per term through the audited type-correct pair(G1,G2) wrapper.
    GT out = gt_one();
    for (std::size_t i = 0; i < A.size(); ++i) {
        out = gt_mul(out, pair(A[i], B[i]));
    }
    return out;
}

GT pair_product_multipairing(
    const std::vector<G1>& A,
    const std::vector<G2>& B) {
    if (A.size() != B.size()) {
        throw std::invalid_argument("pair_product vector lengths differ");
    }
    if (A.empty()) {
        return gt_one();
    }

    // Milestone 7b: mcl accumulates all Miller loops, then performs exactly one
    // final exponentiation. Mathematical result remains product_i e(A_i,B_i).
    GT miller_product;
    mcl::bn::millerLoopVec(
        miller_product, A.data(), B.data(), A.size(), true);
    GT out;
    mcl::bn::finalExp(out, miller_product);
    return out;
}

GT pair_product(
    const std::vector<G1>& A,
    const std::vector<G2>& B,
    PairProductMethod method) {
    switch (method) {
    case PairProductMethod::Simple:
        return pair_product_simple(A, B);
    case PairProductMethod::MultiPairing:
        return pair_product_multipairing(A, B);
    case PairProductMethod::Auto:
        return pair_product(A, B, choose_pair_product_method(A.size()));
    }
    throw std::invalid_argument("unknown pair-product method");
}

PairProductMethod choose_pair_product_method(std::size_t term_count) {
    // Milestone 7b measured policy: size one is effectively tied, while mcl's
    // single-final-exponentiation path wins decisively from two terms onward.
    return term_count < 2U
        ? PairProductMethod::Simple
        : PairProductMethod::MultiPairing;
}

void set_pair_product_method(PairProductMethod method) {
    pair_product_method.store(method, std::memory_order_relaxed);
}

PairProductMethod get_pair_product_method() {
    return pair_product_method.load(std::memory_order_relaxed);
}

} // namespace dory
