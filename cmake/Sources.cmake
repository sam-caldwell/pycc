# Source discovery

# Gather all .cpp files under src/ recursively.
file(GLOB_RECURSE PYCC_ALL_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Separate the application entrypoint
set(PYCC_MAIN ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
list(FILTER PYCC_ALL_SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")
# Exclude clang-tidy plugin sources from the driver library; they are built separately.
list(FILTER PYCC_ALL_SOURCES EXCLUDE REGEX ".*/src/clang-tidy/.*")

set(PYCC_DRIVER_SOURCES ${PYCC_ALL_SOURCES})
