#include "dory/serialization.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace dory {
namespace {

constexpr const char* kCurveTag = "BN254_MCL";
constexpr std::size_t kMaxVectorLength = 1U << 20U;

class Reader {
public:
    explicit Reader(std::string_view input) : stream_(std::string(input)) {}

    std::string token(const char* description) {
        std::string value;
        if (!(stream_ >> value)) {
            throw std::invalid_argument(std::string("truncated serialization at ")
                                        + description);
        }
        return value;
    }

    void expect(const char* expected) {
        if (token(expected) != expected) {
            throw std::invalid_argument(std::string("expected serialization token: ")
                                        + expected);
        }
    }

    std::size_t size(const char* description) {
        const std::string value = token(description);
        std::size_t consumed = 0;
        unsigned long long parsed = 0;
        try {
            parsed = std::stoull(value, &consumed, 10);
        } catch (const std::exception&) {
            throw std::invalid_argument(std::string("invalid size: ") + description);
        }
        if (consumed != value.size() || parsed > kMaxVectorLength
            || parsed > std::numeric_limits<std::size_t>::max()) {
            throw std::invalid_argument(std::string("invalid size: ") + description);
        }
        return static_cast<std::size_t>(parsed);
    }

    void finish() {
        expect("END");
        std::string extra;
        if (stream_ >> extra) {
            throw std::invalid_argument("trailing data after serialization END");
        }
    }

private:
    std::istringstream stream_;
};

void write_header(std::ostringstream& out, const char* version) {
    out << version << '\n' << "curve " << kCurveTag << '\n';
}

void read_header(Reader& reader, const char* version) {
    reader.expect(version);
    reader.expect("curve");
    if (reader.token("curve tag") != kCurveTag) {
        throw std::invalid_argument("serialization curve/backend tag mismatch");
    }
}

template <typename T>
std::string element_hex(const T& value) {
    // PLAN §8a: IoSerializeHexStr delegates canonical encoding, compression,
    // and field/group layout to mcl rather than guessing byte order here.
    return value.serializeToHexStr();
}

template <typename T>
T parse_element(const std::string& encoded, const char* description) {
    if (encoded.empty()) {
        throw std::invalid_argument(std::string("empty element encoding: ") + description);
    }
    T value;
    // PLAN §8a: require mcl to accept and consume the complete hex token.
    const std::size_t consumed = value.deserialize(
        encoded.data(), encoded.size(), mcl::IoSerializeHexStr);
    if (consumed == 0U || consumed != encoded.size()) {
        throw std::invalid_argument(std::string("invalid element encoding: ")
                                    + description);
    }
    return value;
}

template <typename T>
void write_element(std::ostringstream& out, const char* name, const T& value) {
    out << name << ' ' << element_hex(value) << '\n';
}

template <typename T>
T read_element(Reader& reader, const char* name) {
    reader.expect(name);
    return parse_element<T>(reader.token(name), name);
}

void write_statement_body(std::ostringstream& out, const DoryStatement& statement) {
    if (statement.n == 0U || statement.Gamma.size() != statement.n
        || statement.Lambda.size() != statement.n) {
        throw std::invalid_argument("cannot serialize malformed Dory statement");
    }
    out << "n " << statement.n << '\n';
    out << "gamma_count " << statement.Gamma.size() << '\n';
    for (const G1& value : statement.Gamma) write_element(out, "g1", value);
    out << "lambda_count " << statement.Lambda.size() << '\n';
    for (const G2& value : statement.Lambda) write_element(out, "g2", value);
    write_element(out, "Q", statement.Q);
    write_element(out, "D0", statement.D0);
    write_element(out, "D1", statement.D1);
    write_element(out, "D2", statement.D2);
}

DoryStatement read_statement_body(Reader& reader) {
    reader.expect("n");
    DoryStatement statement;
    statement.n = reader.size("n");
    if (statement.n == 0U) throw std::invalid_argument("statement n must be nonzero");
    reader.expect("gamma_count");
    const std::size_t gamma_count = reader.size("gamma_count");
    if (gamma_count != statement.n) throw std::invalid_argument("Gamma length differs from n");
    statement.Gamma.reserve(gamma_count);
    for (std::size_t i = 0; i < gamma_count; ++i) {
        statement.Gamma.push_back(read_element<G1>(reader, "g1"));
    }
    reader.expect("lambda_count");
    const std::size_t lambda_count = reader.size("lambda_count");
    if (lambda_count != statement.n) throw std::invalid_argument("Lambda length differs from n");
    statement.Lambda.reserve(lambda_count);
    for (std::size_t i = 0; i < lambda_count; ++i) {
        statement.Lambda.push_back(read_element<G2>(reader, "g2"));
    }
    statement.Q = read_element<GT>(reader, "Q");
    statement.D0 = read_element<GT>(reader, "D0");
    statement.D1 = read_element<GT>(reader, "D1");
    statement.D2 = read_element<GT>(reader, "D2");
    return statement;
}

void write_scalar_body(std::ostringstream& out, const ScalarProductTranscript& tr) {
    write_element(out, "P1", tr.msg1.P1);
    write_element(out, "P2", tr.msg1.P2);
    write_element(out, "S", tr.msg1.S);
    write_element(out, "R", tr.msg1.R);
    write_element(out, "epsilon", tr.epsilon);
    write_element(out, "E1", tr.msg2.E1);
    write_element(out, "E2", tr.msg2.E2);
    write_element(out, "r1_resp", tr.msg2.r1_resp);
    write_element(out, "r2_resp", tr.msg2.r2_resp);
    write_element(out, "r0_hat", tr.msg2.r0_hat);
    write_element(out, "theta", tr.theta);
}

ScalarProductTranscript read_scalar_body(Reader& reader) {
    ScalarProductTranscript tr;
    tr.msg1.P1 = read_element<GT>(reader, "P1");
    tr.msg1.P2 = read_element<GT>(reader, "P2");
    tr.msg1.S = read_element<GT>(reader, "S");
    tr.msg1.R = read_element<GT>(reader, "R");
    tr.epsilon = read_element<Fr>(reader, "epsilon");
    tr.msg2.E1 = read_element<G1>(reader, "E1");
    tr.msg2.E2 = read_element<G2>(reader, "E2");
    tr.msg2.r1_resp = read_element<Fr>(reader, "r1_resp");
    tr.msg2.r2_resp = read_element<Fr>(reader, "r2_resp");
    tr.msg2.r0_hat = read_element<Fr>(reader, "r0_hat");
    tr.theta = read_element<Fr>(reader, "theta");
    return tr;
}

void write_dory_body(std::ostringstream& out, const DoryTranscript& tr) {
    const std::size_t rounds = tr.round_msg1.size();
    if (tr.beta_challenges.size() != rounds || tr.round_msg2.size() != rounds
        || tr.alpha_challenges.size() != rounds) {
        throw std::invalid_argument("cannot serialize inconsistent Dory transcript");
    }
    out << "rounds " << rounds << '\n';
    for (std::size_t i = 0; i < rounds; ++i) {
        write_element(out, "D1L", tr.round_msg1[i].D1L);
        write_element(out, "D1R", tr.round_msg1[i].D1R);
        write_element(out, "D2L", tr.round_msg1[i].D2L);
        write_element(out, "D2R", tr.round_msg1[i].D2R);
        write_element(out, "beta", tr.beta_challenges[i]);
        write_element(out, "W1", tr.round_msg2[i].W1);
        write_element(out, "W2", tr.round_msg2[i].W2);
        write_element(out, "alpha", tr.alpha_challenges[i]);
    }
    write_scalar_body(out, tr.scalar_transcript);
}

DoryTranscript read_dory_body(Reader& reader) {
    reader.expect("rounds");
    const std::size_t rounds = reader.size("rounds");
    DoryTranscript tr;
    tr.round_msg1.resize(rounds);
    tr.beta_challenges.resize(rounds);
    tr.round_msg2.resize(rounds);
    tr.alpha_challenges.resize(rounds);
    for (std::size_t i = 0; i < rounds; ++i) {
        tr.round_msg1[i].D1L = read_element<GT>(reader, "D1L");
        tr.round_msg1[i].D1R = read_element<GT>(reader, "D1R");
        tr.round_msg1[i].D2L = read_element<GT>(reader, "D2L");
        tr.round_msg1[i].D2R = read_element<GT>(reader, "D2R");
        tr.beta_challenges[i] = read_element<Fr>(reader, "beta");
        tr.round_msg2[i].W1 = read_element<GT>(reader, "W1");
        tr.round_msg2[i].W2 = read_element<GT>(reader, "W2");
        tr.alpha_challenges[i] = read_element<Fr>(reader, "alpha");
    }
    tr.scalar_transcript = read_scalar_body(reader);
    return tr;
}

template <typename T>
std::string serialize_primitive(const T& value, const char* version) {
    std::ostringstream out;
    write_header(out, version);
    write_element(out, "value", value);
    out << "END\n";
    return out.str();
}

template <typename T>
T deserialize_primitive(std::string_view text, const char* version) {
    Reader reader(text);
    read_header(reader, version);
    T value = read_element<T>(reader, "value");
    reader.finish();
    return value;
}

} // namespace

std::string serialize_fr(const Fr& value) { return serialize_primitive(value, "DORY_FR_V1"); }
std::string serialize_g1(const G1& value) { return serialize_primitive(value, "DORY_G1_V1"); }
std::string serialize_g2(const G2& value) { return serialize_primitive(value, "DORY_G2_V1"); }
std::string serialize_gt(const GT& value) { return serialize_primitive(value, "DORY_GT_V1"); }
Fr deserialize_fr(std::string_view text) { return deserialize_primitive<Fr>(text, "DORY_FR_V1"); }
G1 deserialize_g1(std::string_view text) { return deserialize_primitive<G1>(text, "DORY_G1_V1"); }
G2 deserialize_g2(std::string_view text) { return deserialize_primitive<G2>(text, "DORY_G2_V1"); }
GT deserialize_gt(std::string_view text) { return deserialize_primitive<GT>(text, "DORY_GT_V1"); }

std::string serialize_dory_statement(const DoryStatement& statement) {
    std::ostringstream out; write_header(out, "DORY_STATEMENT_V1");
    write_statement_body(out, statement); out << "END\n"; return out.str();
}

DoryStatement deserialize_dory_statement(std::string_view text) {
    Reader reader(text); read_header(reader, "DORY_STATEMENT_V1");
    DoryStatement value = read_statement_body(reader); reader.finish(); return value;
}

std::string serialize_dory_witness(const DoryWitness& witness) {
    if (witness.Omega.empty() || witness.Omega.size() != witness.Theta.size()) {
        throw std::invalid_argument("cannot serialize malformed Dory witness");
    }
    std::ostringstream out; write_header(out, "DORY_WITNESS_V1");
    out << "n " << witness.Omega.size() << '\n';
    for (const G1& value : witness.Omega) write_element(out, "Omega", value);
    for (const G2& value : witness.Theta) write_element(out, "Theta", value);
    write_element(out, "r0", witness.r0); write_element(out, "r1", witness.r1);
    write_element(out, "r2", witness.r2); out << "END\n"; return out.str();
}

DoryWitness deserialize_dory_witness(std::string_view text) {
    Reader reader(text); read_header(reader, "DORY_WITNESS_V1"); reader.expect("n");
    const std::size_t n = reader.size("n");
    if (n == 0U) throw std::invalid_argument("witness n must be nonzero");
    DoryWitness value; value.Omega.reserve(n); value.Theta.reserve(n);
    for (std::size_t i = 0; i < n; ++i) value.Omega.push_back(read_element<G1>(reader, "Omega"));
    for (std::size_t i = 0; i < n; ++i) value.Theta.push_back(read_element<G2>(reader, "Theta"));
    value.r0 = read_element<Fr>(reader, "r0"); value.r1 = read_element<Fr>(reader, "r1");
    value.r2 = read_element<Fr>(reader, "r2"); reader.finish(); return value;
}

std::string serialize_scalar_product_transcript(const ScalarProductTranscript& tr) {
    std::ostringstream out; write_header(out, "DORY_SCALAR_TRANSCRIPT_V1");
    write_scalar_body(out, tr); out << "END\n"; return out.str();
}

ScalarProductTranscript deserialize_scalar_product_transcript(std::string_view text) {
    Reader reader(text); read_header(reader, "DORY_SCALAR_TRANSCRIPT_V1");
    ScalarProductTranscript value = read_scalar_body(reader); reader.finish(); return value;
}

std::string serialize_dory_transcript(const DoryTranscript& tr) {
    std::ostringstream out; write_header(out, "DORY_TRANSCRIPT_V1");
    write_dory_body(out, tr); out << "END\n"; return out.str();
}

DoryTranscript deserialize_dory_transcript(std::string_view text) {
    Reader reader(text); read_header(reader, "DORY_TRANSCRIPT_V1");
    DoryTranscript value = read_dory_body(reader); reader.finish(); return value;
}

std::string serialize_batch_dory_transcript(const BatchDoryTranscript& tr) {
    std::ostringstream out; write_header(out, "DORY_BATCH_TRANSCRIPT_V1");
    out << "folds " << tr.folds.size() << '\n';
    for (const BatchDoryFoldMsg& fold : tr.folds) {
        write_element(out, "Xcross", fold.Xcross); write_element(out, "gamma", fold.gamma);
    }
    write_dory_body(out, tr.final_dory); out << "END\n"; return out.str();
}

BatchDoryTranscript deserialize_batch_dory_transcript(std::string_view text) {
    Reader reader(text); read_header(reader, "DORY_BATCH_TRANSCRIPT_V1"); reader.expect("folds");
    const std::size_t count = reader.size("folds"); BatchDoryTranscript value; value.folds.resize(count);
    for (BatchDoryFoldMsg& fold : value.folds) {
        fold.Xcross = read_element<GT>(reader, "Xcross"); fold.gamma = read_element<Fr>(reader, "gamma");
    }
    value.final_dory = read_dory_body(reader); reader.finish(); return value;
}

std::string serialize_dory_statements(const std::vector<DoryStatement>& statements) {
    if (statements.empty()) throw std::invalid_argument("cannot serialize empty statement list");
    std::ostringstream out; write_header(out, "DORY_STATEMENTS_V1");
    out << "count " << statements.size() << '\n';
    for (const DoryStatement& statement : statements) write_statement_body(out, statement);
    out << "END\n"; return out.str();
}

std::vector<DoryStatement> deserialize_dory_statements(std::string_view text) {
    Reader reader(text); read_header(reader, "DORY_STATEMENTS_V1"); reader.expect("count");
    const std::size_t count = reader.size("count");
    if (count == 0U) throw std::invalid_argument("statement list must be nonempty");
    std::vector<DoryStatement> values; values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) values.push_back(read_statement_body(reader));
    reader.finish(); return values;
}

std::size_t serialized_size_bytes(const DoryTranscript& tr) {
    return serialize_dory_transcript(tr).size();
}
std::size_t serialized_size_bytes(const BatchDoryTranscript& tr) {
    return serialize_batch_dory_transcript(tr).size();
}

} // namespace dory
