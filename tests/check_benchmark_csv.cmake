if(NOT DEFINED BENCH_DORY OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "BENCH_DORY and OUTPUT are required")
endif()

file(REMOVE "${OUTPUT}")
execute_process(
    COMMAND "${BENCH_DORY}" --quick --csv "${OUTPUT}"
    RESULT_VARIABLE benchmark_result
    OUTPUT_VARIABLE benchmark_stdout
    ERROR_VARIABLE benchmark_stderr
)
if(NOT benchmark_result EQUAL 0)
    message(FATAL_ERROR
        "bench_dory failed (${benchmark_result})\n${benchmark_stdout}\n${benchmark_stderr}")
endif()
if(NOT EXISTS "${OUTPUT}")
    message(FATAL_ERROR "bench_dory did not create ${OUTPUT}")
endif()

file(READ "${OUTPUT}" csv)
string(FIND "${csv}" "kind,n,ell,iterations,setup_ms_mean" header_position)
string(FIND "${csv}" "verify_deferred_simple_ms_mean" simple_position)
string(FIND "${csv}" "verify_deferred_windowed_ms_mean" windowed_position)
string(FIND "${csv}" "speedup_simple_over_windowed" speedup_position)
string(FIND "${csv}" "default_method" method_position)
string(FIND "${csv}" "\nsingle," single_position)

foreach(position IN ITEMS
        header_position simple_position windowed_position speedup_position
        method_position single_position)
    if(${position} EQUAL -1)
        message(FATAL_ERROR "CSV output is missing required content: ${position}")
    endif()
endforeach()

message(STATUS "Benchmark CSV check passed: ${OUTPUT}")

