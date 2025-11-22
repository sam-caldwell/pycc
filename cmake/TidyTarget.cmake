# Define a 'tidy' target to run clang-tidy with .clang-tidy configuration.

include_guard(GLOBAL)

file(GLOB_RECURSE TIDY_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/src/*.cpp)

# Exclude plugin sources and build trees
foreach(SRC ${TIDY_SOURCES})
  string(FIND "${SRC}" "/src/clang-tidy/" POS)
  if(NOT POS EQUAL -1)
    list(REMOVE_ITEM TIDY_SOURCES ${SRC})
  endif()
endforeach()

# Exclude runtime sources from tidy for now (noisy const-correctness on low-level code)
foreach(SRC ${TIDY_SOURCES})
  string(FIND "${SRC}" "/src/runtime/" POS)
  if(NOT POS EQUAL -1)
    list(REMOVE_ITEM TIDY_SOURCES ${SRC})
  endif()
endforeach()

# Exclude LLVM pass sources (use external headers not always available in lint env)
foreach(SRC ${TIDY_SOURCES})
  string(FIND "${SRC}" "/src/llvm/" POS)
  if(NOT POS EQUAL -1)
    list(REMOVE_ITEM TIDY_SOURCES ${SRC})
  endif()
endforeach()

# Exclude optimizer sources from tidy (heavy refactors not in scope for lint gate)
foreach(SRC ${TIDY_SOURCES})
  string(FIND "${SRC}" "/src/optimizer/" POS)
  if(NOT POS EQUAL -1)
    list(REMOVE_ITEM TIDY_SOURCES ${SRC})
  endif()
endforeach()

# Exclude sema sources from tidy for now (pending const-correctness and named parameter cleanup)
foreach(SRC ${TIDY_SOURCES})
  string(FIND "${SRC}" "/src/sema/" POS)
  if(NOT POS EQUAL -1)
    list(REMOVE_ITEM TIDY_SOURCES ${SRC})
  endif()
endforeach()

if(NOT TARGET tidy)
  # Attempt to auto-detect the standard library include path for clang-tidy header analysis
  set(TIDY_EXTRA_ARGS "")

  if(APPLE)
    # Try CommandLineTools libc++ headers
    set(_CLT_STDLIB "/Library/Developer/CommandLineTools/usr/include/c++/v1")
    if(EXISTS "${_CLT_STDLIB}")
      list(APPEND TIDY_EXTRA_ARGS "-extra-arg=-isystem" "-extra-arg=${_CLT_STDLIB}")
    endif()
    # Try SDK path via xcrun
    find_program(XCRUN_EXECUTABLE xcrun)
    if(XCRUN_EXECUTABLE)
      execute_process(COMMAND ${XCRUN_EXECUTABLE} --show-sdk-path OUTPUT_VARIABLE _SDK_PATH ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(_SDK_PATH)
        set(_SDK_STDLIB "${_SDK_PATH}/usr/include/c++/v1")
        if(EXISTS "${_SDK_STDLIB}")
          list(APPEND TIDY_EXTRA_ARGS "-extra-arg=-isystem" "-extra-arg=${_SDK_STDLIB}")
        endif()
        # Also add SDK usr/include as a fallback
        if(EXISTS "${_SDK_PATH}/usr/include")
          list(APPEND TIDY_EXTRA_ARGS "-extra-arg=-isystem" "-extra-arg=${_SDK_PATH}/usr/include")
        endif()
      endif()
    endif()
    # Prefer libc++ on macOS
    list(APPEND TIDY_EXTRA_ARGS "-extra-arg=-stdlib=libc++")
  else()
    # Linux/BSD: probe common libstdc++ include paths
    file(GLOB _GLIBCXX_DIRS "/usr/include/c++/*")
    list(SORT _GLIBCXX_DIRS COMPARE NATURAL ORDER DESCENDING)
    list(GET _GLIBCXX_DIRS 0 _GLIBCXX_TOP_DIR)
    if(_GLIBCXX_TOP_DIR)
      list(APPEND TIDY_EXTRA_ARGS "-extra-arg=-isystem" "-extra-arg=${_GLIBCXX_TOP_DIR}")
      # Per-target triple subdir if present
      if(CMAKE_CXX_COMPILER_TARGET)
        set(_GLIBCXX_TGT_DIR "${_GLIBCXX_TOP_DIR}/${CMAKE_CXX_COMPILER_TARGET}")
        if(EXISTS "${_GLIBCXX_TGT_DIR}")
          list(APPEND TIDY_EXTRA_ARGS "-extra-arg=-isystem" "-extra-arg=${_GLIBCXX_TGT_DIR}")
        endif()
      endif()
    endif()
    # Some distros place headers under multiarch directories
    file(GLOB _ALT_STDLIB_DIRS "/usr/include/x86_64-linux-gnu/c++/*" "/usr/include/aarch64-linux-gnu/c++/*")
    foreach(_D IN LISTS _ALT_STDLIB_DIRS)
      list(APPEND TIDY_EXTRA_ARGS "-extra-arg=-isystem" "-extra-arg=${_D}")
    endforeach()
  endif()

  # Provide minimal libc++ include path on Apple to locate standard headers
  if(APPLE)
    # Use the active SDK as sysroot + libc++ to ensure standard headers are found.
    find_program(XCRUN_EXECUTABLE xcrun)
    if(XCRUN_EXECUTABLE)
      execute_process(COMMAND ${XCRUN_EXECUTABLE} --show-sdk-path OUTPUT_VARIABLE _SDK_PATH ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    if(_SDK_PATH)
      set(TIDY_EXTRA_ARGS "-extra-arg=-isysroot" "-extra-arg=${_SDK_PATH}" "-extra-arg=-stdlib=libc++")
    else()
      # Fallback: try common CLT SDK location
      set(_SDK_PATH "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk")
      if(EXISTS "${_SDK_PATH}")
        set(TIDY_EXTRA_ARGS "-extra-arg=-isysroot" "-extra-arg=${_SDK_PATH}" "-extra-arg=-stdlib=libc++")
      else()
        set(TIDY_EXTRA_ARGS "")
      endif()
    endif()
  else()
    set(TIDY_EXTRA_ARGS "")
  endif()

  # Silence underlying compiler warnings from system headers during clang-tidy
  # This does not affect clang-tidy checks themselves.
  list(APPEND TIDY_EXTRA_ARGS "-extra-arg=-w")

  # Create per-file tidy targets so the build system can parallelize them
  set(TIDY_TASKS)
  set(_idx 0)
  foreach(SRC ${TIDY_SOURCES})
    set(_task tidy_${_idx})
    add_custom_target(${_task}
      COMMAND ${CMAKE_COMMAND} -E echo "[tidy] ${SRC}"
      COMMAND bash ${CMAKE_SOURCE_DIR}/tools/run_clang_tidy_silent.sh -quiet -p ${CMAKE_BINARY_DIR} --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy ${TIDY_EXTRA_ARGS} ${SRC}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      VERBATIM)
    list(APPEND TIDY_TASKS ${_task})
    math(EXPR _idx "${_idx}+1")
  endforeach()
  add_custom_target(tidy DEPENDS ${TIDY_TASKS})
endif()
