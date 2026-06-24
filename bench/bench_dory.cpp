#include "dory/dory_batch.hpp"
#include "dory/dory_prover.hpp"
#include "dory/dory_verifier.hpp"
#include "dory/profiling.hpp"
#include "dory/serialization.hpp"
#include "dory/transcript_counts.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Row {
    std::string kind;
    std::size_t n = 0;
    std::size_t ell = 1;
    std::size_t iterations = 0;
    double setup_ms = 0.0;
    double instance_ms = 0.0;
    double prove_ms = 0.0;
    double verify_deferred_simple_ms = 0.0;
    double verify_deferred_windowed_ms = 0.0;
    double verify_eager_ms = -1.0;
    double gt_terms = 0.0;
    std::size_t window_bits = 0;
    std::size_t gt_elements = 0;
    std::size_t g1_elements = 0;
    std::size_t g2_elements = 0;
    std::size_t fr_elements = 0;
    std::size_t serialized_proof_bytes = 0;
    std::size_t serialized_batch_proof_bytes = 0;
    dory::PairingProfile pairing_profile;
    dory::TimingProfile timing_profile;
    std::string pair_product_method;
    std::string setup_method;
};

struct PairProductRow {
    std::size_t size = 0;
    std::size_t iterations = 0;
    double simple_ms = 0.0;
    double multipairing_ms = 0.0;
};

enum class BenchmarkMode {
    Quick,
    Full
};

struct Options {
    BenchmarkMode mode = BenchmarkMode::Quick;
    std::string csv_path;
    std::string profile_csv_path;
    std::string pair_product_csv_path;
    bool show_help = false;
};

void print_help(std::ostream& out) {
    out << "Usage:\n"
           "  bench_dory [--csv <path>] [--profile-csv <path>]\n"
           "             [--pair-product-csv <path>] [--quick] [--full]\n\n"
           "Options:\n"
           "  --csv <path>   Write full benchmark results to a CSV file.\n"
           "  --profile-csv <path>  Write detailed pairing/timing profiles to CSV.\n"
           "  --pair-product-csv <path>  Write direct pair-product timings to CSV.\n"
           "  --quick        Run a short benchmark suitable for checking functionality.\n"
           "  --full         Run the full benchmark suite.\n"
           "  --help         Print this help message.\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    bool mode_was_set = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help") {
            options.show_help = true;
        } else if (argument == "--csv") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--csv requires a path");
            }
            options.csv_path = argv[++i];
        } else if (argument == "--profile-csv") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--profile-csv requires a path");
            }
            options.profile_csv_path = argv[++i];
        } else if (argument == "--pair-product-csv") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--pair-product-csv requires a path");
            }
            options.pair_product_csv_path = argv[++i];
        } else if (argument == "--quick" || argument == "--full") {
            const BenchmarkMode selected = argument == "--quick"
                ? BenchmarkMode::Quick
                : BenchmarkMode::Full;
            if (mode_was_set && options.mode != selected) {
                throw std::invalid_argument("--quick and --full are mutually exclusive");
            }
            options.mode = selected;
            mode_was_set = true;
        } else {
            throw std::invalid_argument("unknown option: " + argument);
        }
    }
    return options;
}

template <typename Function>
double measure_ms(Function&& function) {
    const auto start = Clock::now();
    function();
    const auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::size_t single_iterations(std::size_t n) {
    if (n <= 32U) {
        return 20U;
    }
    if (n <= 128U) {
        return 10U;
    }
    if (n <= 512U) {
        return 3U;
    }
    return 1U;
}

std::size_t batch_iterations(std::size_t n) {
    if (n <= 16U) {
        return 5U;
    }
    if (n <= 64U) {
        return 3U;
    }
    return 1U;
}

Row benchmark_single(
    std::size_t n,
    std::size_t iterations,
    bool profiling,
    dory::DorySetupMethod setup_method) {
    Row row;
    row.kind = "single";
    row.n = n;
    row.iterations = iterations;

    dory::DoryPrecomp pp;
    row.setup_ms = measure_ms([&] {
        pp = profiling
            ? dory::setup_dory_for_tests(
                n, setup_method, &row.pairing_profile, &row.timing_profile)
            : dory::setup_dory_for_tests(n, setup_method, nullptr, nullptr);
    });

    dory::DoryTranscriptCounts last_counts;
    double terms_total = 0.0;
    for (std::size_t iteration = 0; iteration < row.iterations; ++iteration) {
        std::pair<dory::DoryStatement, dory::DoryWitness> instance;
        row.instance_ms += measure_ms([&] {
            instance = profiling
                ? dory::random_valid_dory_instance(
                    pp, &row.pairing_profile, &row.timing_profile)
                : dory::random_valid_dory_instance(pp);
        });

        dory::DoryTranscript transcript;
        dory::DeterministicChallengeChannel channel(
            800000U + static_cast<std::uint64_t>(n) * 100U + iteration);
        row.prove_ms += measure_ms([&] {
            transcript = profiling
                ? dory::prove_dory_interactive(
                    pp, instance.first, instance.second, channel,
                    &row.pairing_profile, &row.timing_profile)
                : dory::prove_dory_interactive(
                    pp, instance.first, instance.second, channel);
        });

        dory::DoryVerifyStats simple_stats;
        bool simple_ok = false;
        row.verify_deferred_simple_ms += measure_ms([&] {
            simple_ok = dory::verify_dory_deferred_multiexp(
                pp, instance.first, transcript, &simple_stats,
                dory::GtMultiexpMethod::Simple);
        });
        require(simple_ok, "bench simple deferred verifier rejected honest proof");

        dory::DoryVerifyStats windowed_stats;
        bool windowed_ok = false;
        row.verify_deferred_windowed_ms += measure_ms([&] {
            windowed_ok = dory::verify_dory_deferred_multiexp(
                pp, instance.first, transcript, &windowed_stats,
                dory::GtMultiexpMethod::WindowedBucket);
        });
        require(windowed_ok, "bench windowed deferred verifier rejected honest proof");
        require(dory::verify_dory(pp, instance.first, transcript),
                "bench default verifier rejected honest proof");

        if (n <= 128U) {
            bool eager_ok = false;
            const double eager_ms = measure_ms([&] {
                eager_ok = dory::verify_dory_eager_reference(
                    pp, instance.first, transcript);
            });
            require(eager_ok, "bench eager verifier rejected honest proof");
            if (row.verify_eager_ms < 0.0) {
                row.verify_eager_ms = 0.0;
            }
            row.verify_eager_ms += eager_ms;
        }

        require(simple_stats.final_check_terms == windowed_stats.final_check_terms,
                "simple/windowed verifier term counts differ");
        terms_total += static_cast<double>(simple_stats.final_check_terms);
        last_counts = dory::count_dory_transcript(transcript);
        row.serialized_proof_bytes += dory::serialized_size_bytes(transcript);
    }

    const double denominator = static_cast<double>(row.iterations);
    row.instance_ms /= denominator;
    row.prove_ms /= denominator;
    row.verify_deferred_simple_ms /= denominator;
    row.verify_deferred_windowed_ms /= denominator;
    if (row.verify_eager_ms >= 0.0) {
        row.verify_eager_ms /= denominator;
    }
    row.gt_terms = terms_total / denominator;
    row.window_bits = dory::gt_multiexp_window_bits(
        static_cast<std::size_t>(row.gt_terms));
    row.gt_elements = last_counts.gt_elements;
    row.g1_elements = last_counts.g1_elements;
    row.g2_elements = last_counts.g2_elements;
    row.fr_elements = last_counts.fr_elements;
    row.serialized_proof_bytes /= row.iterations;
    if (profiling) {
        row.timing_profile.verify_ms =
            dory::choose_gt_multiexp_method(static_cast<std::size_t>(row.gt_terms))
                    == dory::GtMultiexpMethod::Simple
                ? row.verify_deferred_simple_ms
                : row.verify_deferred_windowed_ms;
    }

    std::cout << "single n=" << n
              << " iterations=" << row.iterations
              << " setup_ms=" << row.setup_ms
              << " instance_ms=" << row.instance_ms
              << " prove_ms=" << row.prove_ms
              << " verify_deferred_simple_ms=" << row.verify_deferred_simple_ms
              << " verify_deferred_windowed_ms=" << row.verify_deferred_windowed_ms
              << " verify_eager_ms=" << row.verify_eager_ms
              << " gt_terms=" << row.gt_terms
              << " window_bits=" << row.window_bits
              << " proof_counts={gt:" << row.gt_elements
              << ",g1:" << row.g1_elements
              << ",g2:" << row.g2_elements
              << ",fr:" << row.fr_elements << "}\n";
    return row;
}

Row benchmark_batch(
    std::size_t n,
    std::size_t ell,
    std::size_t iterations,
    bool profiling,
    dory::DorySetupMethod setup_method) {
    Row row;
    row.kind = "batch";
    row.n = n;
    row.ell = ell;
    row.iterations = iterations;

    dory::DoryPrecomp pp;
    row.setup_ms = measure_ms([&] {
        pp = profiling
            ? dory::setup_dory_for_tests(
                n, setup_method, &row.pairing_profile, &row.timing_profile)
            : dory::setup_dory_for_tests(n, setup_method, nullptr, nullptr);
    });

    dory::BatchDoryTranscriptCounts last_counts;
    double terms_total = 0.0;
    for (std::size_t iteration = 0; iteration < row.iterations; ++iteration) {
        std::vector<dory::DoryStatement> statements;
        std::vector<dory::DoryWitness> witnesses;
        row.instance_ms += measure_ms([&] {
            statements.reserve(ell);
            witnesses.reserve(ell);
            for (std::size_t i = 0; i < ell; ++i) {
                auto instance = profiling
                    ? dory::random_valid_dory_instance(
                        pp, &row.pairing_profile, &row.timing_profile)
                    : dory::random_valid_dory_instance(pp);
                statements.push_back(std::move(instance.first));
                witnesses.push_back(std::move(instance.second));
            }
        });

        dory::BatchDoryTranscript transcript;
        dory::DeterministicChallengeChannel channel(
            900000U + static_cast<std::uint64_t>(n) * 1000U
            + static_cast<std::uint64_t>(ell) * 10U + iteration);
        row.prove_ms += measure_ms([&] {
            transcript = profiling
                ? dory::prove_batch_dory(
                    pp, statements, witnesses, channel,
                    &row.pairing_profile, &row.timing_profile)
                : dory::prove_batch_dory(
                    pp, statements, witnesses, channel);
        });

        auto verify_with_method = [&](
            dory::GtMultiexpMethod method,
            dory::DoryVerifyStats* stats) {
            dory::DoryStatement accumulated = statements[0];
            for (std::size_t i = 1U; i < statements.size(); ++i) {
                const dory::BatchDoryFoldMsg& fold = transcript.folds[i - 1U];
                accumulated = dory::combine_batch_statements(
                    accumulated, statements[i], fold.Xcross, fold.gamma);
            }
            return dory::verify_dory_deferred_multiexp(
                pp, accumulated, transcript.final_dory, stats, method);
        };

        dory::DoryVerifyStats simple_stats;
        bool simple_ok = false;
        row.verify_deferred_simple_ms += measure_ms([&] {
            simple_ok = verify_with_method(
                dory::GtMultiexpMethod::Simple, &simple_stats);
        });
        require(simple_ok, "bench simple batch verifier rejected honest proof");

        dory::DoryVerifyStats windowed_stats;
        bool windowed_ok = false;
        row.verify_deferred_windowed_ms += measure_ms([&] {
            windowed_ok = verify_with_method(
                dory::GtMultiexpMethod::WindowedBucket, &windowed_stats);
        });
        require(windowed_ok, "bench windowed batch verifier rejected honest proof");
        require(dory::verify_batch_dory(pp, statements, transcript),
                "bench default batch verifier rejected honest proof");
        require(simple_stats.final_check_terms == windowed_stats.final_check_terms,
                "simple/windowed batch term counts differ");
        terms_total += static_cast<double>(simple_stats.final_check_terms);
        last_counts = dory::count_batch_dory_transcript(transcript);
        row.serialized_batch_proof_bytes += dory::serialized_size_bytes(transcript);
    }

    const double denominator = static_cast<double>(row.iterations);
    row.instance_ms /= denominator;
    row.prove_ms /= denominator;
    row.verify_deferred_simple_ms /= denominator;
    row.verify_deferred_windowed_ms /= denominator;
    row.gt_terms = terms_total / denominator;
    row.window_bits = dory::gt_multiexp_window_bits(
        static_cast<std::size_t>(row.gt_terms));
    row.gt_elements = last_counts.gt_elements;
    row.g1_elements = last_counts.g1_elements;
    row.g2_elements = last_counts.g2_elements;
    row.fr_elements = last_counts.fr_elements;
    row.serialized_batch_proof_bytes /= row.iterations;
    if (profiling) {
        row.timing_profile.verify_ms =
            dory::choose_gt_multiexp_method(static_cast<std::size_t>(row.gt_terms))
                    == dory::GtMultiexpMethod::Simple
                ? row.verify_deferred_simple_ms
                : row.verify_deferred_windowed_ms;
    }

    std::cout << "batch n=" << n
              << " ell=" << ell
              << " iterations=" << row.iterations
              << " setup_ms=" << row.setup_ms
              << " instance_ms=" << row.instance_ms
              << " prove_ms=" << row.prove_ms
              << " verify_simple_ms=" << row.verify_deferred_simple_ms
              << " verify_windowed_ms=" << row.verify_deferred_windowed_ms
              << " batch_folds=" << last_counts.batch_folds
              << " recursive_rounds=" << last_counts.final_dory.recursive_rounds
              << " gt_terms=" << row.gt_terms
              << " window_bits=" << row.window_bits
              << " proof_counts={gt:" << row.gt_elements
              << ",g1:" << row.g1_elements
              << ",g2:" << row.g2_elements
              << ",fr:" << row.fr_elements << "}\n";
    return row;
}

const char* method_name(dory::GtMultiexpMethod method) {
    return method == dory::GtMultiexpMethod::Simple
        ? "Simple"
        : "WindowedBucket";
}

const char* pair_product_method_name(dory::PairProductMethod method) {
    switch (method) {
    case dory::PairProductMethod::Simple:
        return "Simple";
    case dory::PairProductMethod::MultiPairing:
        return "MultiPairing";
    case dory::PairProductMethod::Auto:
        return "Auto";
    }
    return "Unknown";
}

const char* setup_method_name(dory::DorySetupMethod method) {
    return method == dory::DorySetupMethod::SimplePrecomp
        ? "SimplePrecomp"
        : "OptimizedPrecomp";
}

void write_csv(std::ostream& out, const std::vector<Row>& rows) {
    out << std::fixed << std::setprecision(3);
    out << "kind,n,ell,iterations,setup_ms_mean,instance_ms_mean,"
           "prove_ms_mean,verify_deferred_simple_ms_mean,"
           "verify_deferred_windowed_ms_mean,verify_eager_ms_mean,"
           "gt_terms_mean,window_bits,gt_elements,g1_elements,g2_elements,"
           "fr_elements,speedup_simple_over_windowed,default_method,"
           "pair_product_method,setup_method,serialized_proof_bytes,"
           "serialized_batch_proof_bytes\n";
    for (const Row& row : rows) {
        const double speedup = row.verify_deferred_simple_ms
                             / row.verify_deferred_windowed_ms;
        const auto method = dory::choose_gt_multiexp_method(
            static_cast<std::size_t>(row.gt_terms));
        out << row.kind << ',' << row.n << ',' << row.ell << ','
            << row.iterations << ',' << row.setup_ms << ','
            << row.instance_ms << ',' << row.prove_ms << ','
            << row.verify_deferred_simple_ms << ','
            << row.verify_deferred_windowed_ms << ','
            << row.verify_eager_ms << ','
            << row.gt_terms << ',' << row.window_bits << ',' << row.gt_elements << ','
            << row.g1_elements << ',' << row.g2_elements << ','
            << row.fr_elements << ',' << speedup << ',' << method_name(method) << ','
            << row.pair_product_method << ',' << row.setup_method << ','
            << row.serialized_proof_bytes << ','
            << row.serialized_batch_proof_bytes << '\n';
    }
}

void write_profile_csv(std::ostream& out, const std::vector<Row>& rows) {
    out << std::fixed << std::setprecision(3);
    out << "kind,n,ell,iterations,setup_ms_mean,instance_ms_mean,prove_ms_mean,"
           "verify_ms_mean,setup_precompute_ms_mean,instance_commit_ms_mean,"
           "recursive_prover_ms_mean,scalar_prover_ms_mean,batch_fold_ms_mean,"
           "final_dory_prove_ms_mean,setup_pair_terms_mean,"
           "instance_pair_terms_mean,prover_d1_terms_mean,prover_d2_terms_mean,"
           "prover_w_terms_mean,scalar_pair_calls_mean,batch_cross_terms_mean,"
           "total_pair_product_terms_mean,pair_calls_mean,pair_product_calls_mean,"
           "pair_product_method,setup_method\n";

    for (const Row& row : rows) {
        const double iterations = static_cast<double>(row.iterations);
        const auto& p = row.pairing_profile;
        const auto& t = row.timing_profile;
        const double dynamic_terms = static_cast<double>(
            p.pair_product_total_terms - p.setup_pair_product_terms) / iterations;
        const double dynamic_pair_calls = static_cast<double>(
            p.pair_calls - p.setup_pair_product_terms - 1U) / iterations;
        const double dynamic_product_calls = static_cast<double>(
            p.pair_product_calls - p.setup_pair_product_calls) / iterations;

        out << row.kind << ',' << row.n << ',' << row.ell << ',' << row.iterations
            << ',' << row.setup_ms << ',' << row.instance_ms << ',' << row.prove_ms
            << ',' << t.verify_ms << ',' << t.setup_precompute_ms << ','
            << t.instance_commit_ms / iterations << ','
            << t.recursive_prover_ms / iterations << ','
            << t.scalar_prover_ms / iterations << ','
            << t.batch_fold_ms / iterations << ','
            << t.final_dory_prove_ms / iterations << ','
            << p.setup_pair_product_terms << ','
            << static_cast<double>(p.instance_pair_product_terms) / iterations << ','
            << static_cast<double>(p.prover_d1_pair_product_terms) / iterations << ','
            << static_cast<double>(p.prover_d2_pair_product_terms) / iterations << ','
            << static_cast<double>(p.prover_w_pair_product_terms) / iterations << ','
            << static_cast<double>(p.scalar_pair_calls) / iterations << ','
            << static_cast<double>(p.batch_cross_pair_product_terms) / iterations << ','
            << static_cast<double>(p.setup_pair_product_terms) + dynamic_terms << ','
            << static_cast<double>(p.setup_pair_product_terms + 1U)
                + dynamic_pair_calls << ','
            << static_cast<double>(p.setup_pair_product_calls) + dynamic_product_calls
            << ',' << row.pair_product_method << ',' << row.setup_method
            << '\n';
    }
}

std::size_t pair_product_iterations(std::size_t size, BenchmarkMode mode) {
    if (mode == BenchmarkMode::Quick) {
        return size <= 32U ? 3U : 1U;
    }
    if (size <= 32U) {
        return 20U;
    }
    if (size <= 128U) {
        return 10U;
    }
    if (size <= 512U) {
        return 3U;
    }
    return 1U;
}

std::vector<PairProductRow> benchmark_pair_products(BenchmarkMode mode) {
    constexpr std::array<std::size_t, 11> sizes{
        {1U, 2U, 4U, 8U, 16U, 32U, 64U, 128U, 256U, 512U, 1024U}};
    std::vector<PairProductRow> rows;
    for (const std::size_t size : sizes) {
        PairProductRow row;
        row.size = size;
        row.iterations = pair_product_iterations(size, mode);
        std::vector<dory::G1> A;
        std::vector<dory::G2> B;
        A.reserve(size);
        B.reserve(size);
        for (std::size_t i = 0; i < size; ++i) {
            A.push_back(dory::random_g1_for_tests());
            B.push_back(dory::random_g2_for_tests());
        }
        for (std::size_t i = 0; i < row.iterations; ++i) {
            dory::GT simple;
            row.simple_ms += measure_ms([&] {
                simple = dory::pair_product_simple(A, B);
            });
            dory::GT multi;
            row.multipairing_ms += measure_ms([&] {
                multi = dory::pair_product_multipairing(A, B);
            });
            require(dory::gt_equal(simple, multi),
                    "pair-product benchmark methods disagree");
        }
        row.simple_ms /= static_cast<double>(row.iterations);
        row.multipairing_ms /= static_cast<double>(row.iterations);
        std::cout << "pair_product size=" << size
                  << " iterations=" << row.iterations
                  << " simple_ms=" << row.simple_ms
                  << " multipairing_ms=" << row.multipairing_ms
                  << " speedup=" << row.simple_ms / row.multipairing_ms << '\n';
        rows.push_back(row);
    }
    return rows;
}

void write_pair_product_csv(
    std::ostream& out,
    const std::vector<PairProductRow>& rows) {
    out << std::fixed << std::setprecision(3);
    out << "pair_product_size,iterations,simple_ms_mean,multipairing_ms_mean,"
           "speedup_simple_over_multipairing\n";
    for (const PairProductRow& row : rows) {
        out << row.size << ',' << row.iterations << ',' << row.simple_ms << ','
            << row.multipairing_ms << ',' << row.simple_ms / row.multipairing_ms
            << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        if (options.show_help) {
            print_help(std::cout);
            return 0;
        }

        dory::Backend::init_bn254();
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Dory benchmark over BN254/mcl ("
                  << (options.mode == BenchmarkMode::Quick ? "quick" : "full")
                  << ")\n";
        std::vector<Row> rows;
        const bool profiling = !options.profile_csv_path.empty();

        dory::set_pair_product_method(dory::PairProductMethod::Auto);
        auto add_single_rows = [&](std::size_t n, std::size_t iterations) {
            for (const auto setup_method : {dory::DorySetupMethod::SimplePrecomp,
                                            dory::DorySetupMethod::OptimizedPrecomp}) {
                std::cout << "Setup method: " << setup_method_name(setup_method) << '\n';
                Row row = benchmark_single(
                    n, iterations, profiling, setup_method);
                row.pair_product_method = "Auto";
                row.setup_method = setup_method_name(setup_method);
                rows.push_back(std::move(row));
            }
        };
        auto add_batch_rows = [&](
            std::size_t n,
            std::size_t ell,
            std::size_t iterations) {
            for (const auto setup_method : {dory::DorySetupMethod::SimplePrecomp,
                                            dory::DorySetupMethod::OptimizedPrecomp}) {
                std::cout << "Setup method: " << setup_method_name(setup_method) << '\n';
                Row row = benchmark_batch(
                    n, ell, iterations, profiling, setup_method);
                row.pair_product_method = "Auto";
                row.setup_method = setup_method_name(setup_method);
                rows.push_back(std::move(row));
            }
        };

        if (options.mode == BenchmarkMode::Quick) {
            constexpr std::array<std::size_t, 3> quick_single{{1U, 16U, 64U}};
            for (const std::size_t n : quick_single) {
                add_single_rows(n, 2U);
            }
            constexpr std::array<std::size_t, 2> quick_ells{{1U, 4U}};
            for (const std::size_t ell : quick_ells) {
                add_batch_rows(16U, ell, 1U);
            }
        } else {
            constexpr std::array<std::size_t, 11> single_sizes{
                {1U, 2U, 4U, 8U, 16U, 32U, 64U, 128U, 256U, 512U, 1024U}};
            for (const std::size_t n : single_sizes) {
                add_single_rows(n, single_iterations(n));
            }

            constexpr std::array<std::size_t, 3> batch_ns{{16U, 64U, 256U}};
            constexpr std::array<std::size_t, 5> batch_ells{{1U, 2U, 4U, 8U, 16U}};
            for (const std::size_t n : batch_ns) {
                for (const std::size_t ell : batch_ells) {
                    add_batch_rows(n, ell, batch_iterations(n));
                }
            }
        }
        dory::set_pair_product_method(dory::PairProductMethod::Auto);

        const std::vector<PairProductRow> pair_product_rows =
            benchmark_pair_products(options.mode);

        if (!options.csv_path.empty()) {
            std::ofstream csv(options.csv_path, std::ios::out | std::ios::trunc);
            if (!csv) {
                throw std::runtime_error("could not open CSV path: " + options.csv_path);
            }
            write_csv(csv, rows);
            csv.close();
            std::cout << "CSV written to: "
                      << std::filesystem::absolute(options.csv_path).string() << '\n';
        } else {
            std::cout << "No CSV requested; use --csv <path> to save full results.\n";
        }


        if (!options.profile_csv_path.empty()) {
            std::ofstream profile_csv(
                options.profile_csv_path, std::ios::out | std::ios::trunc);
            if (!profile_csv) {
                throw std::runtime_error(
                    "could not open profile CSV path: " + options.profile_csv_path);
            }
            write_profile_csv(profile_csv, rows);
            profile_csv.close();
            std::cout << "Profile summary:\n";
            for (const Row& row : rows) {
                const double iterations = static_cast<double>(row.iterations);
                std::cout << "  " << row.kind << " n=" << row.n
                          << " ell=" << row.ell
                          << " setup_method=" << row.setup_method
                          << " setup_terms="
                          << row.pairing_profile.setup_pair_product_terms
                          << " instance_terms="
                          << row.pairing_profile.instance_pair_product_terms / iterations
                          << " prover_d1/d2/w_terms="
                          << row.pairing_profile.prover_d1_pair_product_terms / iterations
                          << '/'
                          << row.pairing_profile.prover_d2_pair_product_terms / iterations
                          << '/'
                          << row.pairing_profile.prover_w_pair_product_terms / iterations
                          << " scalar_pairs="
                          << row.pairing_profile.scalar_pair_calls / iterations
                          << " batch_cross_terms="
                          << row.pairing_profile.batch_cross_pair_product_terms / iterations
                          << '\n';
            }
            std::cout << "Profile CSV written to: "
                      << std::filesystem::absolute(options.profile_csv_path).string()
                      << '\n';
        }

        if (!options.pair_product_csv_path.empty()) {
            std::ofstream pair_csv(
                options.pair_product_csv_path, std::ios::out | std::ios::trunc);
            if (!pair_csv) {
                throw std::runtime_error(
                    "could not open pair-product CSV path: "
                    + options.pair_product_csv_path);
            }
            write_pair_product_csv(pair_csv, pair_product_rows);
            pair_csv.close();
            std::cout << "Pair-product CSV written to: "
                      << std::filesystem::absolute(options.pair_product_csv_path).string()
                      << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Dory benchmark failure: " << error.what() << '\n';
        return 1;
    }
}
