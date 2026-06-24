# Dory over BN254

## Project overview

This C++17 project implements the Dory inner-product protocol using the
herumi/mcl BN254 backend. It uses a Type-III pairing `e : G1 x G2 -> GT`, with
additive notation for `G1`/`G2` and multiplicative notation for `GT`.

## Build on Windows

Run from the project root in a Visual Studio Developer command prompt:

```bat
conda activate base
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR=%CONDA_PREFIX%\Library
cmake --build build --config Release
```

The project path may contain spaces. Run commands from the project root or
quote absolute paths.

## Run tests

```bat
ctest --test-dir build --output-on-failure
```

## Run a demo proof

```bat
.\build\dory_demo.exe prove --n 16 --proof proof.dory --statement statement.dory
.\build\dory_demo.exe verify --proof proof.dory --statement statement.dory
```

## Run a batch demo proof

```bat
.\build\dory_demo.exe prove-batch --n 16 --ell 4 --proof batch_proof.dory --statements batch_statements.dory
.\build\dory_demo.exe verify-batch --proof batch_proof.dory --statements batch_statements.dory
```

Demo files use the versioned, curve-tagged serialization format.

## Run a quick benchmark

```bat
.\build\bench_dory.exe --quick --csv bench_results.csv --profile-csv bench_profile.csv
```

See [BENCHMARK.md](BENCHMARK.md) for full benchmark and Visual Studio generator
commands.

## Library API usage

The facade in `dory/dory_api.hpp` wraps existing setup, prover, verifier, and
size helpers without printing to stdout.

```cpp
dory::DoryRunMetrics setup_metrics;
auto pp = dory::make_dory_crs_for_size(n, &setup_metrics);
auto [stmt, wit] = dory::random_valid_dory_instance(pp);
dory::RandomChallengeChannel channel;
auto proof = dory::prove_dory_single(pp, stmt, wit, channel);

dory::DoryRunMetrics verify_metrics;
bool ok = dory::verify_dory_single(
    pp, stmt, proof.proof, &verify_metrics);
```

Batch callers use `prove_dory_batch_api` and `verify_dory_batch_api`. One
compatible `DoryPrecomp` can be reused across multiple proofs.

## Current limitations

- This is an interactive/test implementation, not a Fiat-Shamir proof system.
- The domain-separated public test setup requires production review.
- BN254 is suitable for prototyping; serious deployments should consider a
  higher-security curve.
- Serialized proof files are untrusted input and must continue to be validated.
