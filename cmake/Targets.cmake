# Targets and build settings

# Library with all implementation sources except main
add_library(pycc_driver STATIC ${PYCC_DRIVER_SOURCES})
target_include_directories(pycc_driver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(pycc_driver PRIVATE cxx_std_23)
target_compile_definitions(pycc_driver PRIVATE
  PYCC_EMIT_LLVM=$<BOOL:${PYCC_EMIT_LLVM}>
  PYCC_EMIT_ASM=$<BOOL:${PYCC_EMIT_ASM}>)

# Application executable
add_executable(pycc ${PYCC_MAIN})
target_include_directories(pycc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(pycc PRIVATE cxx_std_23)

# Propagate emission toggles to the app
target_compile_definitions(pycc PRIVATE
  PYCC_EMIT_LLVM=$<BOOL:${PYCC_EMIT_LLVM}>
  PYCC_EMIT_ASM=$<BOOL:${PYCC_EMIT_ASM}>)

# Warnings
if(MSVC)
  target_compile_options(pycc PRIVATE /W4 /permissive-)
else()
  target_compile_options(pycc PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(pycc PRIVATE pycc_driver)
