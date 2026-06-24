@echo off
setlocal

if "%CONDA_PREFIX%"=="" (
  echo Warning: CONDA_PREFIX is not set. Activate your conda environment first.
)

cmake --build build --config Release
if errorlevel 1 exit /b %errorlevel%

ctest --test-dir build --output-on-failure
if errorlevel 1 exit /b %errorlevel%

.\build\bench_dory.exe --full --csv bench_results.csv --profile-csv bench_profile.csv --pair-product-csv pair_product_microbench.csv
if errorlevel 1 exit /b %errorlevel%

echo Benchmark CSV written to bench_results.csv
echo Benchmark profile CSV written to bench_profile.csv
echo Pair-product microbenchmark CSV written to pair_product_microbench.csv
