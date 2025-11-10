# CTest script to compile and run a tiny Python program with pycc and check exit status

if(NOT DEFINED pycc_exe)
  message(FATAL_ERROR "pycc_exe is not defined")
endif()

set(sample_py "${CMAKE_CURRENT_BINARY_DIR}/sample.py")
set(out_bin   "${CMAKE_CURRENT_BINARY_DIR}/e2e_app")

file(WRITE "${sample_py}" "def main() -> int:\n    return 7\n")

execute_process(
  COMMAND "${pycc_exe}" -o "${out_bin}" "${sample_py}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "pycc failed (rc=${rc})\nstdout:\n${out}\nstderr:\n${err}")
endif()

if(NOT EXISTS "${out_bin}")
  message(FATAL_ERROR "expected output binary not found: ${out_bin}")
endif()

execute_process(
  COMMAND "${out_bin}"
  RESULT_VARIABLE rc2
  OUTPUT_VARIABLE out2
  ERROR_VARIABLE err2
)
if(NOT rc2 EQUAL 7)
  message(FATAL_ERROR "unexpected exit code: ${rc2}\nstdout:\n${out2}\nstderr:\n${err2}")
endif()

