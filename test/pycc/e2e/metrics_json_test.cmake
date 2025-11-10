# CTest script: run pycc with --metrics=json and validate JSON output

if(NOT DEFINED pycc_exe)
  message(FATAL_ERROR "pycc_exe is not defined")
endif()

set(sample_py "${CMAKE_CURRENT_BINARY_DIR}/sample_json.py")
set(out_bin   "${CMAKE_CURRENT_BINARY_DIR}/e2e_json_app")

file(WRITE "${sample_py}" "def main() -> int:\n    return 3\n")

execute_process(
  COMMAND "${pycc_exe}" --metrics=json -o "${out_bin}" "${sample_py}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "pycc failed (rc=${rc})\nstdout:\n${out}\nstderr:\n${err}")
endif()

# Basic JSON sanity: starts with '{' and contains key durations_ns
string(REGEX MATCH "^\n?\{" match1 "${out}")
if(NOT match1)
  message(FATAL_ERROR "metrics output not JSON object start\n${out}")
endif()
string(FIND "${out}" "\"durations_ns\"" pos)
if(pos LESS 0)
  message(FATAL_ERROR "metrics JSON missing durations_ns key\n${out}")
endif()

