# Dory benchmarks on Windows

Run commands from the project root. The project path contains a space, so quote
absolute paths if invoking these commands from another directory.

`bench_dory` defaults to the short `--quick` suite. Add `--full` for the complete
single and batch matrix used for performance comparisons.

## Ninja build

```bat
conda activate <your-env>
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR=%CONDA_PREFIX%\Library
cmake --build build --config Release
ctest --test-dir build --output-on-failure
.\build\bench_dory.exe --quick --csv bench_results.csv
```
## conda activate base
## %CONDA_PREFIX% = C:\Users\anh001\AppData\Local\miniconda3


Run the complete benchmark matrix with:

```bat
.\build\bench_dory.exe --full --csv bench_results_full.csv
```

Collect detailed pairing and nested timing profiles with:

```bat
.\build\bench_dory.exe --full --csv bench_results_full.csv --profile-csv bench_profile_full.csv --pair-product-csv pair_product_microbench.csv
```

## Visual Studio generator

```bat
conda activate <your-env>
cmake -S . -B build_vs -G "Visual Studio 17 2022" -A x64 -DOPENSSL_ROOT_DIR=%CONDA_PREFIX%\Library
cmake --build build_vs --config Release
ctest --test-dir build_vs -C Release --output-on-failure
.\build_vs\Release\bench_dory.exe --quick --csv bench_results.csv
```

Run the complete benchmark matrix with:

```bat
.\build_vs\Release\bench_dory.exe --full --csv bench_results_full.csv
```

For a profiled Visual Studio run:

```bat
.\build_vs\Release\bench_dory.exe --full --csv bench_results_full.csv --profile-csv bench_profile_full.csv --pair-product-csv pair_product_microbench.csv
```

## Help

```bat
.\build\bench_dory.exe --help
```

CSV paths are resolved relative to the directory from which the executable is
run. With the commands above, `bench_results.csv` is written to the project root.
