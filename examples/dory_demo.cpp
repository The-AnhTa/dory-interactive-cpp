#include "dory/dory_batch.hpp"
#include "dory/dory_api.hpp"
#include "dory/dory_prover.hpp"
#include "dory/dory_verifier.hpp"
#include "dory/serialization.hpp"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string command;
    std::size_t n = 0;
    std::size_t ell = 0;
    std::string proof;
    std::string statement;
};

void usage() {
    std::cout << "Usage:\n"
              << "  dory_demo prove --n <n> --proof <file> --statement <file>\n"
              << "  dory_demo verify --proof <file> --statement <file>\n"
              << "  dory_demo prove-batch --n <n> --ell <count> --proof <file> --statements <file>\n"
              << "  dory_demo verify-batch --proof <file> --statements <file>\n";
}

std::size_t parse_size(const std::string& text, const char* option) {
    std::size_t consumed = 0;
    const unsigned long long value = std::stoull(text, &consumed, 10);
    if (consumed != text.size() || value == 0U) throw std::invalid_argument(std::string("invalid ") + option);
    return static_cast<std::size_t>(value);
}

Options parse(int argc, char** argv) {
    if (argc < 2) throw std::invalid_argument("missing command");
    Options out; out.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (i + 1 >= argc) throw std::invalid_argument(arg + " requires a value");
        const std::string value = argv[++i];
        if (arg == "--n") out.n = parse_size(value, "--n");
        else if (arg == "--ell") out.ell = parse_size(value, "--ell");
        else if (arg == "--proof") out.proof = value;
        else if (arg == "--statement" || arg == "--statements") out.statement = value;
        else throw std::invalid_argument("unknown option: " + arg);
    }
    if (out.proof.empty() || out.statement.empty()) throw std::invalid_argument("proof and statement paths are required");
    if ((out.command == "prove" || out.command == "prove-batch") && out.n == 0U) {
        throw std::invalid_argument("--n is required for proving");
    }
    if (out.command == "prove-batch" && out.ell == 0U) throw std::invalid_argument("--ell is required");
    return out;
}

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open input file: " + path);
    std::ostringstream contents; contents << input.rdbuf(); return contents.str();
}

void write_file(const std::string& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot open output file: " + path);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output) throw std::runtime_error("failed writing output file: " + path);
}

dory::DoryPrecomp precomp_for(const dory::DoryStatement& statement) {
    return dory::setup_dory_from_generators(statement.Gamma, statement.Lambda, statement.Q);
}

int prove(const Options& options) {
    dory::DoryRunMetrics setup_metrics;
    const dory::DoryPrecomp pp = dory::make_dory_crs_for_size(
        options.n, &setup_metrics);
    const auto instance = dory::random_valid_dory_instance(pp);
    dory::RandomChallengeChannel channel;
    dory::DorySingleProof result = dory::prove_dory_single(
        pp, instance.first, instance.second, channel);
    dory::DoryRunMetrics verify_metrics;
    const bool accepted = dory::verify_dory_single(
        pp, result.statement, result.proof, &verify_metrics);
    const std::string encoded = dory::serialize_dory_transcript(result.proof);
    write_file(options.proof, encoded);
    write_file(options.statement, dory::serialize_dory_statement(result.statement));
    std::cout << std::fixed << std::setprecision(3)
              << "DORY_RUN kind=single n=" << options.n << " ell=1"
              << " precompute_ms=" << setup_metrics.precompute_ms
              << " crs_bytes=" << setup_metrics.crs_bytes
              << " prove_ms=" << result.metrics.prove_ms
              << " proof_bytes=" << result.metrics.proof_bytes
              << " verify_ms=" << verify_metrics.verify_ms
              << " accepted=" << (accepted ? 1 : 0) << '\n';
    return accepted ? EXIT_SUCCESS : EXIT_FAILURE;
}

int verify(const Options& options) {
    const dory::DoryStatement statement = dory::deserialize_dory_statement(read_file(options.statement));
    const dory::DoryTranscript proof = dory::deserialize_dory_transcript(read_file(options.proof));
    const bool accepted = dory::verify_dory(precomp_for(statement), statement, proof);
    std::cout << "verification = " << (accepted ? "ACCEPT" : "REJECT") << '\n';
    return accepted ? EXIT_SUCCESS : EXIT_FAILURE;
}

int prove_batch(const Options& options) {
    dory::DoryRunMetrics setup_metrics;
    const dory::DoryPrecomp pp = dory::make_dory_crs_for_size(
        options.n, &setup_metrics);
    std::vector<dory::DoryStatement> statements;
    std::vector<dory::DoryWitness> witnesses;
    for (std::size_t i = 0; i < options.ell; ++i) {
        auto instance = dory::random_valid_dory_instance(pp);
        statements.push_back(std::move(instance.first)); witnesses.push_back(std::move(instance.second));
    }
    dory::RandomChallengeChannel channel;
    dory::DoryBatchProof result = dory::prove_dory_batch_api(
        pp, statements, witnesses, channel);
    dory::DoryRunMetrics verify_metrics;
    const bool accepted = dory::verify_dory_batch_api(
        pp, result.statements, result.proof, &verify_metrics);
    const std::string encoded = dory::serialize_batch_dory_transcript(result.proof);
    write_file(options.proof, encoded); write_file(options.statement, dory::serialize_dory_statements(statements));
    std::cout << std::fixed << std::setprecision(3)
              << "DORY_RUN kind=batch n=" << options.n
              << " ell=" << options.ell
              << " precompute_ms=" << setup_metrics.precompute_ms
              << " crs_bytes=" << setup_metrics.crs_bytes
              << " prove_ms=" << result.metrics.prove_ms
              << " proof_bytes=" << result.metrics.proof_bytes
              << " verify_ms=" << verify_metrics.verify_ms
              << " accepted=" << (accepted ? 1 : 0) << '\n';
    return accepted ? EXIT_SUCCESS : EXIT_FAILURE;
}

int verify_batch(const Options& options) {
    const auto statements = dory::deserialize_dory_statements(read_file(options.statement));
    const auto proof = dory::deserialize_batch_dory_transcript(read_file(options.proof));
    const bool accepted = dory::verify_batch_dory(precomp_for(statements.front()), statements, proof);
    std::cout << "verification = " << (accepted ? "ACCEPT" : "REJECT") << '\n';
    return accepted ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace

int main(int argc, char** argv) {
    try {
        dory::Backend::init_bn254();
        const Options options = parse(argc, argv);
        if (options.command == "prove") return prove(options);
        if (options.command == "verify") return verify(options);
        if (options.command == "prove-batch") return prove_batch(options);
        if (options.command == "verify-batch") return verify_batch(options);
        usage(); throw std::invalid_argument("unknown command: " + options.command);
    } catch (const std::exception& error) {
        std::cerr << "dory_demo: " << error.what() << '\n'; usage(); return EXIT_FAILURE;
    }
}
