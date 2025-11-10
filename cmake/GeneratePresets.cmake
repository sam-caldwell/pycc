# (c) 2025 Sam Caldwell. See LICENSE.txt
# cmake/GeneratePresets.cmake
# Generate a basic CMakePresets.json if one does not already exist.

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(PRESETS_FILE "${ROOT}/CMakePresets.json")

if(EXISTS "${PRESETS_FILE}")
  message(STATUS "CMakePresets.json already exists; skipping generation.")
  return()
endif()

file(WRITE "${PRESETS_FILE}" [[
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
  "configurePresets": [
    {
      "name": "default",
      "displayName": "Default (Ninja)",
      "generator": "Ninja",
      "binaryDir": "build",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": { "type": "STRING", "value": "Debug" },
        "CMAKE_C_COMPILER": { "type": "STRING", "value": "$env{CMAKE_C_COMPILER}" },
        "CMAKE_CXX_COMPILER": { "type": "STRING", "value": "$env{CMAKE_CXX_COMPILER}" },
        "PYCC_EMIT_LLVM": { "type": "BOOL", "value": true },
        "PYCC_EMIT_ASM": { "type": "BOOL", "value": true }
      }
    }
  ],
  "buildPresets": [
    { "name": "default", "configurePreset": "default" }
  ],
  "testPresets": [
    { "name": "default", "configurePreset": "default", "output": { "outputOnFailure": true } }
  ]
}
]])

message(STATUS "Generated ${PRESETS_FILE}")

