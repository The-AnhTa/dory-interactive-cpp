#pragma once

#include "dory/dory_batch.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace dory {

// PLAN §8a: versioned, curve-tagged textual encodings. All curve elements are
// encoded through mcl's documented serialization API, never through object memory.
std::string serialize_fr(const Fr& value);
std::string serialize_g1(const G1& value);
std::string serialize_g2(const G2& value);
std::string serialize_gt(const GT& value);

Fr deserialize_fr(std::string_view text);
G1 deserialize_g1(std::string_view text);
G2 deserialize_g2(std::string_view text);
GT deserialize_gt(std::string_view text);

std::string serialize_dory_statement(const DoryStatement& statement);
std::string serialize_dory_witness(const DoryWitness& witness);
std::string serialize_scalar_product_transcript(
    const ScalarProductTranscript& transcript);
std::string serialize_dory_transcript(const DoryTranscript& transcript);
std::string serialize_batch_dory_transcript(
    const BatchDoryTranscript& transcript);
std::string serialize_dory_statements(
    const std::vector<DoryStatement>& statements);

DoryStatement deserialize_dory_statement(std::string_view text);
DoryWitness deserialize_dory_witness(std::string_view text);
ScalarProductTranscript deserialize_scalar_product_transcript(
    std::string_view text);
DoryTranscript deserialize_dory_transcript(std::string_view text);
BatchDoryTranscript deserialize_batch_dory_transcript(std::string_view text);
std::vector<DoryStatement> deserialize_dory_statements(std::string_view text);

std::size_t serialized_size_bytes(const DoryTranscript& transcript);
std::size_t serialized_size_bytes(const BatchDoryTranscript& transcript);

} // namespace dory
