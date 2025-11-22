include_guard(GLOBAL)

file(GLOB_RECURSE PYCC_HEADERS CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/include/**/*.h)

file(GLOB_RECURSE PYCC_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/**/*.cpp)

# Exclude clang-tidy plugin sources from the main compiler build
foreach(SRC ${PYCC_SOURCES})
  string(FIND "${SRC}" "/src/clang-tidy/" POS)
  if(NOT POS EQUAL -1)
    list(REMOVE_ITEM PYCC_SOURCES ${SRC})
  endif()
endforeach()

# Exclude LLVM pass sources from core; they are built as a separate plugin target
foreach(SRC ${PYCC_SOURCES})
  string(FIND "${SRC}" "/src/llvm/ElideGCBarrierPass.cpp" POS_LLVM_PASS)
  if(NOT POS_LLVM_PASS EQUAL -1)
    list(REMOVE_ITEM PYCC_SOURCES ${SRC})
  endif()
endforeach()

# Separate CLI main from core for re-use in tests
set(PYCC_MAIN ${CMAKE_SOURCE_DIR}/src/cli/Main.cpp)
list(REMOVE_ITEM PYCC_SOURCES ${PYCC_MAIN})
